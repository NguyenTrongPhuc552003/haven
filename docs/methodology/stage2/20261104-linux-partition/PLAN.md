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
