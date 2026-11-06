# Linux in a HAVEN partition: plan

**Status:** PLAN, 2026-11-04, approved by the owner ("Approve", item 3 of
the follow-up). Written by Claude. Expectations in section 4 are
Claude's, fixed before the first run; they are not an oracle.

## 1. Problem

KDAL is meant to run in a Linux partition under HAVEN, with its reported
demand checked against the partition's grant and read-back checking the
same grant on the hardware (the E/D/G/T/H chain of the charter). Today
both HAVEN partitions run small bare-metal demo guests. Without a Linux
partition the chain exists only as files (`../20261104-ffa-grant/`).

## 2. What Linux needs from HAVEN (from the arm64 boot protocol and the
## code as it is; inference until each step is observed)

| Need | HAVEN today | Work |
| - | - | - |
| Boot: Image at a 2 MiB aligned address, MMU off, EL1, `x0` = device tree address | ERET to the partition base with `x0` = DMA grant bits | a build option that passes a device tree address in `x0` |
| Device tree for the partition | none (demo guests hard-code addresses) | a partition DTS: memory, one CPU, PSCI (HVC), GICv3, timer, PL011 |
| GIC distributor and redistributor (MMIO) | not mapped for guests; HAVEN owns the physical GIC; guests use the virtual CPU interface (ICH list registers) | **trap and emulate** GICD and the partition CPU's GICR: the registers Linux's GICv3 driver touches, mapped onto the interrupts the partition owns |
| PSCI through HVC (version, features, CPU_ON, SYSTEM_OFF) | HVC not handled | an HVC handler: version and features, SYSTEM_OFF and SYSTEM_RESET stop the partition, CPU_ON denied (one vCPU first) |
| Timer | virtual timer PPI 27 delivered as a hardware-linked virtual interrupt (partition B) | reuse for partition A's CPU |
| Console | PL011 at 0x09000000 and SPI 33 already granted to partition A | none |
| MMIO emulation in general | data aborts are logged and skipped (`ELR += 4`), loads return whatever the register held | decode the faulting load or store (ESR ISV, SRT, SAS, WnR) with the guest's full register frame |

Trusted-code budget: the GIC emulation and the HVC handler add to HAVEN's
trusted code; keeping them small and read-back-checkable is part of the
research question (read-back checks what is enforced, including what the
emulation programs into the physical GIC).

## 3. Steps (smallest first)

- **L0** boot Linux v6.18 (ELMOS demo kernel) as partition A with a device
  tree and `earlycon`; no other change. Purpose: see how far it gets and
  which accesses fault first.
- **L1** HVC handler (PSCI minimal).
- **L2** MMIO emulation core (decode, full register frame).
- **L3** GICv3 distributor and redistributor emulation for one vCPU.
- **L4** Linux reaches a shell from an initramfs; KDAL loads; its demand
  is checked against the partition's grant; read-back on the same grant.

## 4. L0 expectations (Claude's, before the run)

1. Linux prints its banner through `earlycon` (PL011 is mapped).
2. The first PSCI call (HVC) does not behave: HAVEN has no handler, so
   Linux sees a wrong PSCI version and may fall back or warn.
3. The GICv3 driver's first GICD access (IPA 0x08000000) is a stage-2
   fault; HAVEN logs "Denied stage-2 violation IPA=0x8000000" and skips
   it; the driver then reads garbage and either fails to probe or panics
   ("no irqchip" or a timer failure).
4. HAVEN itself stays up (other CPUs unaffected).

## 5. L1 design and expectations (Claude's, before the run)

Observed in L0 (`RESULTS-L0.md`): an HVC from Linux returns to the guest
with `x0` unchanged, so Linux printed `PSCIv33792.0` (the function ID read
back as a version).

Design (sources: Arm DEN0022 PSCI for the function IDs and return codes;
Linux v6.18 `drivers/firmware/psci/psci.c` for what Linux calls):

- A pure function `hv_psci_call()` in `src/core/exc/psci.c` takes `x0` to
  `x3` and returns the value for `x0` and an action (resume or stop the
  partition). It has host unit tests (`tests/unit/test_psci.c`).
- `hv_handle_sync()` sends EC 0x16 (HVC from AArch64) to it. HVC returns
  to the next instruction, so `ELR_EL2` is left alone. EC 0x17 (SMC
  trapped by `HCR_EL2.TSC`) is handled the same way with `ELR_EL2 += 4`,
  because a trapped SMC returns to the SMC itself (today an SMC from a
  guest would trap again forever).
- PSCI version 1.0. `PSCI_VERSION`, `PSCI_FEATURES`, `MIGRATE_INFO_TYPE`
  (2: no Trusted OS needs migrating), `AFFINITY_INFO` (ON for the
  caller's own MPIDR, INVALID_PARAMETERS otherwise), `CPU_SUSPEND`
  (returns SUCCESS at once, which the spec allows as a wake-up),
  `CPU_ON` (DENIED: one vCPU), `CPU_OFF`, `SYSTEM_OFF` and `SYSTEM_RESET`
  (stop the partition: its CPU is parked at EL2, as for an instruction
  abort). Everything else, including `SMCCC_VERSION` and
  `SYSTEM_RESET2`, is NOT_SUPPORTED (-1), so Linux keeps SMCCC v1.0 and
  makes no other SMCCC calls.
- Each call is logged on the HAVEN console (`HAVEN: PSCI ...`).

Expectations for the L1 run (same images and device tree as L0):

1. Linux prints `psci: PSCIv1.0 detected in firmware.`,
   `psci: Using standard PSCI v0.2 function IDs`,
   `psci: Trusted OS migration not required` and
   `psci: SMC Calling Convention v1.0`.
2. HAVEN logs the calls in this order: PSCI_VERSION, MIGRATE_INFO_TYPE,
   then PSCI_FEATURES for SMCCC_VERSION, CPU_SUSPEND, SYSTEM_SUSPEND,
   SYSTEM_RESET2 and SYSTEM_OFF2 (the order of `psci_probe()`).
3. The GIC still fails as in L0 (L1 adds no MMIO emulation), and Linux
   still panics at the root mount.
4. With `panic=-1` the panic ends in a restart, which Linux sends as
   PSCI SYSTEM_RESET; HAVEN logs it and stops partition A, and Linux no
   longer prints `Reboot failed -- System halted`. HAVEN stays up. (In
   the qemu-direct profile partition B is not started at all: HAVEN's
   own CPU_ON to firmware fails, as the L0 log shows.)

## 6. L2 design and expectations (Claude's, before the run)

Observed in L1: the GIC driver's first access is a stage-2 data abort with
`ESR=0x93880006` at IPA 0x800f000 (FAR ends in 0xfe8, so the register is
GICD_PIDR2 at offset 0xffe8). HAVEN logs it, skips the instruction and
leaves the destination register as it was.

Design (source for the syndrome fields: Arm ARM, ESR_EL2 ISS encoding for a
data abort; ISV bit 24, SAS 23:22, SSE 21, SRT 20:16, SF 15, WnR 6):

- The exception frame saves all general registers: x19 to x28 are added
  at frame+192..264 (the frame grows from 192 to 272 bytes; the existing
  offsets, ELR at +168 and SPSR at +176, do not move). A load can then
  write any destination register, and a store can read any source.
- A pure decoder `hv_mmio_decode()` (`src/core/exc/mmio.c`, host-tested)
  turns the ESR into size, sign extension, register number and direction,
  and computes the value a load writes back (zero- or sign-extended,
  32-bit when SF is 0; register 31 is XZR).
- Emulated regions are registered with `hv_mmio_register()` (IPA range,
  owning CPU, handler). L2 registers none; L3 registers the GIC.
- A decodable access to an unregistered address is still denied and
  logged, with the decoded fields; a load now reads as zero (RAZ) instead
  of leaving the register unchanged, and a store is ignored (WI). An access
  with ISV = 0 (for example a load pair) cannot be emulated and is handled
  as before.

Expectations for the L2 run (same images and device tree as L1):

1. HAVEN logs the GICD_PIDR2 access decoded as a 4-byte read into w8,
   predicted from L1's syndrome 0x93880006 (SAS = 2, SRT = 8, SF = 0,
   WnR = 0), at IPA 0x800ffe8.
2. Linux still prints `no distributor detected, giving up` (PIDR2 now reads
   0, which is not GICv3), and the rest of the boot is as in L1: no timer
   interrupt, root-mount panic, SYSTEM_RESET stops partition A.
3. All host tests pass, including new decoder tests; partitions that never
   fault (qemu-psci profile boot check in CI) behave as before.

## 7. L3 design and expectations (Claude's, before the run)

What Linux v6.18's GICv3 driver needs (from
`drivers/irqchip/irq-gic-v3.c`, inference until observed): GICD_PIDR2 and
GICR_PIDR2 report architecture version 3; GICD_TYPER gives the number of
interrupt lines; GICD_CTLR and GICR_CTLR read RWP = 0; GICR_TYPER (a
64-bit read) carries the CPU's affinity and Last = 1; GICR_WAKER clears
ChildrenAsleep when ProcessorSleep is cleared; the enable, priority,
configuration, group and routing registers take writes. Interrupt
delivery itself already exists: HAVEN injects an owned physical interrupt
as a hardware-linked virtual one, and the guest acknowledges it through
the virtual CPU interface (ICV system registers), not through MMIO.

Design:

- `src/core/irq/vgic.c`: a distributor and one redistributor (RD and SGI
  frames) for one vCPU, registered on the L2 path at GICD 0x08000000
  (64 KiB) and GICR 0x080a0000 (128 KiB), for the Linux profile only
  (`HAVEN_PART_A_DTB_IPA`). It reports 64 interrupt IDs (32 SPIs),
  security disabled (DS = 1), no LPIs, no extended SPIs. Pure state
  machine with host tests; the hardware is reached through hooks.
- Enable and disable of an interrupt that the partition owns on this CPU
  go to the physical GIC; all other interrupt IDs read as zero and ignore
  writes, except SGIs, which are kept as virtual state only. Owned
  interrupts start physically disabled and are enabled when Linux enables
  them. Priority, configuration, group and routing are stored and read
  back but not applied to the hardware (injection keeps priority 0xa0,
  the value Linux writes by default).
- Partition A also owns the EL1 virtual timer PPI 27 on CPU 0 in the
  Linux profile (partition B keeps PPI 27 on CPU 2; a PPI is owned per
  CPU).
- Writes to ICC_SGI1R_EL1 trap to EL2 (EC 0x18). A self-targeted SGI is
  injected as a virtual SGI; other targets are dropped (one vCPU). Any
  other trapped system register reads as zero and ignores writes, with
  one log line. Without this, the first trapped access would trap again
  forever (ELR is not advanced today).
- HAVEN counts virtual interrupts delivered per interrupt ID and prints
  the counts when the partition stops.

Expectations for the L3 run (same images and device tree as L0 to L2):

1. The GIC driver probes: Linux reports 32 SPIs and finds the
   redistributor for CPU 0 at 0x080a0000; no `no distributor detected`.
2. The architected timer registers (virtual timer) and printk timestamps
   advance past 0.000000.
3. At the stop, HAVEN reports more than zero deliveries of interrupt 27
   (timer). Interrupt 33 (UART) may stay at zero: the kernel console
   writes the UART by polling, and there is no user space to read it.
4. Linux still panics at the root mount (no root file system yet) and
   SYSTEM_RESET stops partition A. No stage-2 access is denied.
5. All host tests pass on the three CI operating systems; the qemu-psci
   profile (partition B with its timer) is unchanged.
