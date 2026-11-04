# L0 results: Linux v6.18 as HAVEN partition A, no emulation yet

**Run:** 2026-11-04, macOS, QEMU virt (cortex-a72, qemu-direct profile),
`run_l0.sh` with ELMOS's demo kernel (`demo-linux-v6.18-1`, Image
SHA-256 in `runs/l0/SHA256SUMS`). HAVEN commit: the one that adds
`HAVEN_PART_A_DTB_IPA` (this record's commit). Raw output:
`runs/l0/uart.log`.

## Observations

- HAVEN started as usual, passed the device tree IPA (0x50000000) in
  `x0` and entered partition A.
- Linux printed its banner through `earlycon` and kept going for 254
  lines: memory 512 MiB at IPA 0x40000000 found, one CPU, PL011 driver
  registered.
- PSCI: `psci: PSCIv33792.0 detected in firmware`, `MIGRATE_INFO_TYPE
  returned unknown type`, `SMC Calling Convention v32768.0`. 33792.0 is
  0x84000000, the PSCI_VERSION function ID: HVC returned with `x0`
  unchanged.
- GIC: one stage-2 fault, `IPA=0x800f000 FAR=...ffe8` (offset 0xFFE8 of
  the distributor, GICD_PIDR2, the version probe). HAVEN skipped it, Linux
  printed `GICv3: ... no distributor detected, giving up` and continued
  without an interrupt controller.
- Timer: `arch_timer: No interrupt available, giving up`; every later
  timestamp is 0.000000 (no tick).
- End: `VFS: Unable to mount root fs` (no root device or initramfs given),
  kernel panic, `Reboot failed -- System halted` (PSCI SYSTEM_RESET did
  nothing).
- HAVEN stayed up; no HAVEN error.

## Against the expectations fixed in PLAN.md

| Expectation | Outcome |
| - | - |
| 1. banner through earlycon | as expected |
| 2. PSCI misbehaves | as expected (version read as 33792.0) |
| 3. first GICD access faults, driver fails or panics | faulted as expected; the driver **gave up without a panic** (prediction said fail or panic) |
| 4. HAVEN stays up | as expected |

Not predicted: Linux runs to the root mount with no interrupt controller
and no timer interrupt at all.

## What this means for the next steps

- L1 (HVC/PSCI) is confirmed necessary: Linux depends on PSCI_VERSION and
  SYSTEM_RESET/SYSTEM_OFF.
- L3 (GIC emulation) is confirmed as the main piece: without it there is
  no timer and no console interrupt.
- The order of accesses is visible now: the first is GICD_PIDR2; the
  driver's next reads (GICD_TYPER, the redistributor walk) are what L3
  must serve first.
