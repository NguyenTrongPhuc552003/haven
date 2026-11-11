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
