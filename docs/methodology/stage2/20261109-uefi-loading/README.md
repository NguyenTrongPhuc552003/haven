# L0 and L1: a read-only UEFI probe, and the UEFI path rehearsed in QEMU

**Status:** design and expectations, 2026-11-11, merged before any code
(owner approval 2026-11-09 of `../../../porting/cix-p1-loading.md`: option A2,
a separate `haven-boot.efi`; L0 and L1 as next steps). No board. Labels:
**[F]** fact (file and line, specification), **[I]** inference, **[H]**
hypothesis, **[P]** proposal. Results go in a later section, added by a
later pull request; the expectations below are not edited after the runs.

**Order note (disclosed):** while this record waited for CI (the runners'
DNS outage of 2026-11-11), a draft of the L0 probe was built and run once
in QEMU on the macOS host, before this record merged. The expectations
below were written before that run and were not changed by it; the only
later edit is row T2's runner version (from CI's configuration, not from
the run). The results section reports that run separately from the runs
made after the merge.

## Problem

On the Orange Pi 6 Plus HAVEN will be started by an EFI application, not
by QEMU's `-device loader`. Three things are unknown and every later step
depends on them: the CPU state an EFI application gets (exception level,
HCR_EL2, MMU), the memory map it sees, and whether our toolchains can
produce an AArch64 EFI application at all. L0 answers the first two on any
UEFI machine with a program that changes nothing; L1 runs it in QEMU in CI,
then (L1b) rehearses the full hand-off to the unchanged qemu-virt HAVEN.

## Design

### L0: `haven-probe.efi` [P]

- Freestanding C, no EDK2 or gnu-efi: the few UEFI types it uses (system
  table, boot services `GetMemoryMap`, simple text output, configuration
  table) declared in one header from the UEFI 2.10 specification (sections
  4.3, 4.4, 7.2, 12.4). Built position independent, with no data that
  needs relocation.
- Prints to the UEFI console (on QEMU and on the board it reaches the
  serial port), one `key value` per line, prefixed `probe:`: `CurrentEL`;
  at EL2 `HCR_EL2`, `SCTLR_EL2`, `TCR_EL2`, `CNTHCTL_EL2` (else `n/a`);
  `ID_AA64MMFR1_EL1` (VH field), `ID_AA64MMFR4_EL1` (E2H0 field, read by
  its encoding `S3_0_C0_C7_4`, which reads as zero where not implemented
  [F, Arm ARM, ID register space]), `ID_AA64PFR0_EL1`, `MPIDR_EL1`,
  `MIDR_EL1`, `CNTFRQ_EL0`, `DAIF`, the probe's own load address; every
  memory-map descriptor (type, physical start, pages, attributes); every
  configuration table, with the ACPI 2.0, device tree and SMBIOS 3 GUIDs
  named. First line `probe: HAVEN-PROBE 1`, last line `probe: end`.
- Changes nothing: no ExitBootServices, no allocation kept, no write to a
  system register or device. It returns `EFI_SUCCESS` to the shell.
- Location: `boot/efi/` (source, header, linker script, build script). Not
  part of the `haven` target: HAVEN's image and trusted-code count do not
  change.

### Toolchain question, settled by experiment

1. **objcopy**: link an ELF, then `objcopy -O pei-aarch64-little
   --subsystem efi-app`. Needs binutils with AArch64 PE support. The macOS
   cross binutils is 2.45 and lists `pei-aarch64-little` [F, `objcopy
   --info`, 2026-11-11]; CI's Ubuntu `binutils-aarch64-linux-gnu` is checked
   by the run.
2. **Hand-written header**: a PE/COFF header in assembly at the start of
   the image, as Linux (`arch/arm64/kernel/efi-header.S`) and Xen do. Needs
   no PE support in binutils.

Rule: use (1) if it builds on both toolchains and EDK2 starts the result in
QEMU on both; otherwise (2). The results state which and why.

### L1: QEMU with UEFI at EL2, in CI

- QEMU `virt` with `virtualization=on`, `gic-version=3`, `-cpu max`, `-m
  2G`, UEFI firmware from the host's package (Ubuntu `qemu-efi-aarch64`:
  `QEMU_EFI.fd`; Homebrew: `edk2-aarch64-code.fd`), a FAT drive made from a
  directory (`-drive file=fat:rw:DIR`) holding the probe and a `startup.nsh`
  that runs it, then `reset -s`.
- Script `scripts/qemu/uefi-probe-check.sh BUILD`: boots, keeps the serial
  log in `BUILD/evidence/uefi-probe-uart.log`, checks the rows below, exits
  0 only if all hold. A CI step in the ARM64 job runs it.
- **L1b, loader rehearsal (after L0's results):** `haven-boot.efi` loads
  `haven.bin`, `guest_a.bin` and `guest_b.bin` of the qemu-direct build to
  the addresses `qemu-direct-check.sh` uses (0x8000_0000, 0x8080_0000,
  0xA080_0000), claiming each range with `AllocatePages(AllocateAddress)`,
  then ExitBootServices, clean to PoC, SCTLR_EL2.M and C off, jump. The
  qemu-direct HAVEN is not changed.

## Expectations

| Row | Check | Expected |
| - | - | - |
| T1 | objcopy route, macOS (binutils 2.45) | links; the output is a PE32+ EFI application for AArch64 (`file(1)`) |
| T2 | objcopy route, CI Ubuntu binutils | the same [H: CI's Linux runners are pinned to Ubuntu 24.04 (pull request 67), whose binutils (2.42) is newer than 2.38, the release that added AArch64 PE support]; if not, the hand-written header is used and both rows say so |
| P1 | probe starts in QEMU from `startup.nsh` | `probe: HAVEN-PROBE 1` and `probe: end` in the log; QEMU exits by itself after `reset -s` within 120 s |
| P2 | exception level | `CurrentEL 2` [I: QEMU's EDK2 runs at EL2 with `virtualization=on`] |
| P3 | HCR_EL2 | bit 27 (TGE) set, bit 34 (E2H) clear [F for EDK2 ArmPkg: `ArmExceptionLib/AArch64/AArch64Exception.c` ORs TGE in, nothing in ArmPkg sets E2H; the packaged firmware is not built by us, so the run is the check] |
| P4 | SCTLR_EL2 | M (bit 0), C (bit 2) and I (bit 12) set [F, UEFI 2.10 section 2.3.6] |
| P5 | other registers | `MPIDR_EL1` 0x80000000 (CPU 0); `CNTFRQ_EL0` non-zero; VH = 1 in `ID_AA64MMFR1_EL1` for `-cpu max` |
| P6 | memory map | at least one conventional-memory descriptor inside 0x4000_0000 to 0xC000_0000 (QEMU RAM); the log shows whether 0x8000_0000 to 0xA100_0000 is free, which L1b needs |
| P7 | configuration tables | ACPI 2.0 or device tree (at least one) listed |
| P8 | nothing changed | the shell runs the next `startup.nsh` line after the probe (`reset -s`), so boot services were still alive |
| C1 | CI | the new step passes in the ARM64 job (Ubuntu 24.04); the existing jobs are unchanged |

L1b expectations are added to this record in a new section after L0's
results and before the loader code, because they depend on P6.

## What this does not show

Anything about the Orange Pi 6 Plus: QEMU's EDK2 is not CIX's firmware. The
same probe on the board is probe P2 of `cix-p1-loading.md`.

## Results: L0 (2026-11-13)

Code: pull request #82 (merge `a87c80b`). Evidence: the CI job "ARM64
build and QEMU boot" of that pull request, step "UEFI probe at EL2 in QEMU
(L0, L1)" (Ubuntu 24.04, QEMU 8.2.2, Ubuntu's `QEMU_EFI.fd`), kept in
`runs/ci-ubuntu2404-qemu822.txt`.

**Order of work.** The probe was run once on the Mac (QEMU 11.1.3,
Homebrew's EDK2) on 2026-11-11 at 21:36 local time, before this record's
expectations were merged (#68, 2026-11-12). The expectations had been
written before that run and were not changed after it, except T2's runner
assumption, which was corrected from what CI showed (Ubuntu 26.04 at the
time, pinned back to 24.04 by #67), not from the probe. That run's output
is kept as `runs/mac-qemu1113-before-record.txt`; the verdicts below come
from the CI run.

| Row | Observed | Verdict |
| - | - | - |
| T1 | macOS binutils 2.45: `objcopy -O pei-aarch64-little --subsystem efi-app` gave an AArch64 EFI application (Mac run, before the record) | as expected (observed before the record merged) |
| T2 | CI: `GNU objcopy (GNU Binutils for Ubuntu) 2.42`; `PE32+ executable (EFI application) Aarch64`, 38,162 bytes | as expected: the objcopy route works; no hand-written header needed |
| P1 | `probe: HAVEN-PROBE 1` and `probe: end`; QEMU exited with 0 | as expected |
| P2 | `CurrentEL 0x2` | as expected |
| P3 | `HCR_EL2 0x8000038`: TGE (bit 27) set, E2H (bit 34) clear; also FMO, IMO, AMO (bits 3 to 5) set | as expected |
| P4 | `SCTLR_EL2 0x100d`: M, C, I set | as expected |
| P5 | `MPIDR_EL1 0x80000000`; `CNTFRQ_EL0 0x3b9aca0` (62.5 MHz); `ID_AA64MMFR1_EL1` VH = 1 | as expected |
| P6 | conventional memory 0x4800_0000 to 0xBC30_E000 (0x7430E pages), so 0x8000_0000 to 0xA100_0000 is free | as expected; L1b's addresses are free in CI too |
| P7 | `table ACPI20` and `table SMBIOS3` listed (no device tree with `-cpu max` defaults) | as expected |
| P8 | QEMU exited 0 after `reset -s` | as expected |
| C1 | the new step passed; the other jobs passed unchanged | as expected |

Observations outside the rows:

- `CNTFRQ_EL0` differs between the two QEMUs: 62.5 MHz in CI (QEMU 8.2.2)
  and 1 GHz on the Mac (QEMU 11.1.3). The board's own value is probe P2 of
  `../../../porting/cix-p1-loading.md`.
- The firmware loaded the probe at 0xBC73_8194 in CI and 0xBCB4_C190 on the
  Mac: an image without relocation entries but built with `-fpie` is loaded
  wherever the firmware likes. The loader (L1b) must therefore not assume
  its own address, and claims HAVEN's fixed addresses with
  `AllocatePages(AllocateAddress)` as the loading study proposes.

L1b (the loader rehearsal) gets its expectations in a new section of this
record before any loader code.

## L1b: the loader rehearsed in QEMU (design and expectations, 2026-11-13)

**Status:** design and expectations, merged before any loader code and
before the entry checks in `boot.S` (owner's list of 2026-11-13: L1b and
`haven-boot.efi`). Results go in a later section; the rows below are not
edited after the runs. No loader code has been written or run before this
section merged.

### What changes from the L1b sketch above [P]

The design section above names the qemu-direct build. The rehearsal uses
the **qemu-psci** build instead: same platform (`src/platform/qemu-virt/`),
same addresses, same `haven.bin` link address 0x8000_0000, but HAVEN starts
partition B on CPU 2 through QEMU's PSCI, so the run shows both partitions.
qemu-direct never starts B (its PSCI conduit is "none"). [F] Under UEFI the
conduit stays QEMU's own SMC: QEMU v8.2.2 `hw/arm/virt.c` lines 2104-2110
choose SMC whenever EL2 exists and `secure=on` is not set, firmware or not,
and `hw/arm/boot.c` lines 1278-1293 then start every secondary CPU powered
off.

### `haven-boot.efi` [P]

Freestanding C plus one assembly hand-off routine in `boot/efi/`, built by
`boot/efi/build.sh` next to the probe, with the same rules (position
independent, no relocations; the build also fails if the ELF has a `.got`).
It is not part of the `haven` target. Layout per platform from a small
header that includes HAVEN's own `src/platform/<platform>/memory.h`, so the
loader and HAVEN cannot disagree on an address; today only qemu-virt.

In order:

1. Print `haven-boot: HAVEN-BOOT 1`. If `CurrentEL` is not 2, print it and
   return `EFI_UNSUPPORTED` to the shell (nothing claimed).
2. Claim every range with `AllocatePages(AllocateAddress)` [F, UEFI 2.10
   section 7.2; type numbers from EDK2 `MdePkg/Include/Uefi/UefiSpec.h`
   lines 33-42, data `cix-bios`'s `edk2`]: HAVEN's range as `EfiLoaderCode`,
   the partition, shared and read-only windows as `EfiLoaderData`. On qemu-virt
   (`memory.h`): `haven` 0x8000_0000 + 8 MiB, `part-a` 0x8080_0000 +
   512 MiB, `part-b` 0xA080_0000 + 64 MiB, `shared` 0xA480_0000 + 4 MiB,
   `part-a-ro` 0xA4C0_0000 + 2 MiB. Any failure: free what was claimed,
   print, return to the shell.
3. Zero every claimed range, so HAVEN and the guests start from zeroed RAM
   as with QEMU's `-device loader`, and no firmware data is left in a
   partition window.
4. Read `haven.bin`, `guest_a.bin`, `guest_b.bin` from the volume the loader
   itself came from (loaded image, simple file system protocols) straight
   into their ranges; a file larger than its range is an error. Print each
   file's address, size and SHA-256.
5. Get the memory map and call ExitBootServices; on `EFI_INVALID_PARAMETER`
   (stale map key) get a fresh map and retry, at most 8 times. After the
   first call nothing but `GetMemoryMap` and `ExitBootServices` is used;
   if every try fails the loader parks (it cannot return safely).
6. From here the loader writes to the platform's console UART directly
   (qemu-virt: PL011 0x0900_0000, `QEMU_UART_BASE`): one line with the
   number of tries.
7. Clean and invalidate every claimed range and the loader's own image to
   the point of coherency (`dc civac` by line, line size from CTR_EL0),
   then `ic ialluis`.
8. Assembly hand-off: mask DAIF; clear SCTLR_EL2.M and C; `tlbi alle2`;
   print `haven-boot: jump` on the UART (from the cleaned image, MMU off);
   x0 to x3 = 0; `br` to HAVEN's link address. Linux v5.15
   `arch/arm64/kernel/efi-entry.S` lines 26-67 is the model [F]; x0 = 0
   because HAVEN ignores it and QEMU's loader also leaves it 0.

Two test-only builds of the same source, `haven-boot-seed-mmu.efi` (step 8
leaves the MMU and data cache on) and `haven-boot-seed-e2h.efi` (step 8
sets HCR_EL2.E2H before the jump), exist only to show that the entry checks
refuse. They are built and run by CI and are never meant for a board.

### Entry checks in `arch/arm64/boot.S` [P]

Right after the existing CurrentEL check of the boot CPU, before the stack
or any memory write: if HCR_EL2.E2H (bit 34) is 1, branch to `_entry_e2h`;
if SCTLR_EL2.M (bit 0) is 1, branch to `_entry_mmu`. Each is its own
`wfi` loop, so a stopped CPU's PC tells which check refused. The checks
are in every platform's image (qemu-virt and cix-p1). Secondary CPUs are
not checked: PSCI CPU_ON starts them with the MMU off, and their entry
already re-checks the EL.

Behaviour of the existing images [I]: QEMU's `-device loader` starts CPU 0
from reset (HCR_EL2 = 0, SCTLR_EL2.M = 0), and the L0 run shows E2H = 0
under EDK2, so no existing run is refused. Bytes [I]: `.boot.text` is 0x1E8
bytes and `.entry.text` starts at the next 2 KiB boundary (0x800), so the
added instructions shift nothing after offset 0x800.

### Check script and CI [P]

`scripts/qemu/uefi-boot-check.py BUILD EFIDIR CPU [seconds] [--seed mmu|e2h]`:
builds an ESP directory (the loader, the three images, a `startup.nsh`
running `fs0:`, the loader, then `reset -s` in case the loader returns),
starts QEMU `virt,virtualization=on,gic-version=3,iommu=smmuv3`, `-smp 4
-m 2G`, the UEFI firmware as pflash, and the ESP as a virtio-blk PCI disk
at slot 5 (`addr=5`), so PCI function 00:01.0, which `memory.h` grants to
partition A as its DMA device, stays empty as in the existing checks. It
reuses `partition-b-check.py`'s QMP reader for partition B's status record
and adds the loader and HAVEN rows below. With `--seed` it instead waits
for the loader's jump line and reads CPU 0's PC over QMP (`info
registers`) and compares it with the park symbol in `haven.elf`.

CI: in the ARM64 job, the L0 step is extended (the qemu-psci build already
exists in that job): rehearsal on `cortex-a72` and `max`, seed `mmu` on
`max`, seed `e2h` on `max` only (cortex-a72 has no VHE, so E2H is RES0 there
and the seed could not set it).

### Expectations

Runs: CI, Ubuntu 24.04, QEMU 8.2.2, Ubuntu's `QEMU_EFI.fd` (the versions in
`ecosystem.lock.json` and the L0 results). The Mac is not used for L1b.

| Row | Check | Expected |
| - | - | - |
| B1 | loader build | `haven-boot.efi` and both seed builds are PE32+ EFI applications for AArch64; no relocation section, no `.got` |
| B2 | image bytes | built with the same `SOURCE_DATE_EPOCH`, `haven.bin` of qemu-direct, qemu-psci and cix-p1 keeps its size (45,008, 45,008 and 43,536 bytes on main `20aa20f`); every byte from offset 0x800 on is unchanged; bytes below 0x800 differ |
| B3 | existing checks | every existing CI job and step passes unchanged (qemu-direct boot, partition B, reproducibility, cix-p1 builds, L0 probe, Linux partition, host tests) |
| B4 | trusted code (`tcb-image-loc.py`) | `boot.S` 162 code lines now; qemu 66 files, 7,543 on main, expected 7,551 to 7,561 (+8 to +18); cix-p1 66 files, 7,531 on main, expected 7,539 to 7,549 (+8 to +18). The loader adds nothing to `haven.elf` |
| B5 | loader size (cloc, `boot/efi/` minus the probe; not in `haven.elf`, but in the trusted boot path) | 250 to 450 code lines |
| U1 | loader start | `haven-boot: HAVEN-BOOT 1`, CurrentEL 2, the five ranges claimed at the addresses listed above |
| U2 | files | three `load` lines; addresses as above; sizes and SHA-256 equal to the files on the ESP (checked by the script) |
| U3 | ExitBootServices | succeeds; 1 try [I: nothing runs between the loader's `GetMemoryMap` and `ExitBootServices`; the retry is for firmware that changes the map meanwhile] |
| U4 | MMU-off marker | `haven-boot: jump` after the tries line |
| U5 | HAVEN starts | `Haven hypervisor starting` after the jump line, on both CPU models |
| U6 | partition A | `Hello from partition A`, `Denied stage-2 violation`, `PARTITION_A: Post-fault check OK`, `PARTITION_A: IRQ test done` |
| U7 | partition B | all ten checks of `partition-b-check.py` hold (CPU 2 started, B reaches stage 5, 8 or more timer interrupts, no other interrupts) [H: EDK2 leaves the secondary CPUs off; `ArmVirtQemu.dsc` in `cix-bios`'s `edk2` builds no MP services driver] |
| U8 | time | each rehearsal run under 120 s |
| K1 | seed `mmu` (max) | no `Haven hypervisor starting`; CPU 0's PC inside `_entry_mmu` [H: HAVEN's range is executable in UEFI's map because it is claimed as `EfiLoaderCode`; EDK2's NX policy for ArmVirt, `ArmVirt.dsc.inc` line 371, leaves loader code executable] |
| K2 | seed `e2h` (max) | no `Haven hypervisor starting`; CPU 0's PC inside `_entry_e2h` |
| C1 | CI | the extended step passes; total ARM64 job time grows by less than 5 minutes |

### What this will not show

- Cache maintenance: QEMU's TCG does not model data caches, so a missing or
  wrong clean in steps 7 and 8 would pass here. Without the M check, the
  seed `mmu` HAVEN might even run [I]. Only the board shows the sequence is
  right (probe P8 of the loading study).
- Anything about CIX's firmware: memory map, EL, watchdog and display are
  board probes P1 to P7.
- [O, from L0] EDK2 leaves CNTHCTL_EL2 = 0x3 (EL1 may read the physical
  counter and use the EL1 physical timer without a trap), and HAVEN never
  writes CNTHCTL_EL2 (no write in `arch/` or `src/`), so the value a
  partition gets depends on whoever entered HAVEN. Not judged here; listed
  for the owner as an open question (HAVEN setting CNTHCTL_EL2 itself is an
  architectural change, not part of L1b).
