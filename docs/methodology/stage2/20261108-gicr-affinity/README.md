# D2: redistributors found by affinity, not by logical CPU and a fixed stride

**Status:** step D2 of `../../../porting/cix-p1-device-tree.md` (device tree
preparation, owner-approved 2026-11-08). Design and expectations by Claude,
committed before the code.

## Problem (ledger row 25)

`drivers/irqchip/gic_v3.c` takes CPU n's redistributor at
`gicr_base + n x GICR_STRIDE`, with n HAVEN's logical CPU index and
`GICR_STRIDE` fixed at 128 KiB. That holds in QEMU (logical CPU n is MPIDR
Aff0 = n, frames 128 KiB apart) and on the i.MX boards as configured. On
CIX P1 the vendor tree gives GIC-700 redistributors 256 KiB apart (GICv4
frames), and HAVEN's logical CPU 0 is the boot core (core 4 per the
CPU-map note), so HAVEN would program another core's redistributor.

## Design

A pure function, `hv_gicr_find_frames()` in `src/core/irq/gicr_frames.c`,
walks the redistributor region as the architecture describes it: read
GICR_TYPER at each frame, step 256 KiB if GICR_TYPER.VLPIS is set and
128 KiB otherwise, stop after the frame with GICR_TYPER.Last (or at a
bound of 64 frames), and record for each logical CPU the frame whose
GICR_TYPER affinity equals that CPU's MPIDR affinity. The driver calls it
in `gic_v3_init` with the platform's CPU map and uses the table for every
redistributor access. A CPU with no frame is skipped at init (logged) and
its PPI calls return `HV_EINVAL`, instead of writing to a wrong address.
The fixed `GICR_STRIDE` is no longer used by the driver.

## Expectations

| Check | Expected |
| - | - |
| Unit `test_gicr_frames`: CIX-like region (12 frames, 256 KiB, VLPIS, Aff1 = core, Last on frame 11), logical map with core 4 first | logical CPU 0 gets frame 4 (base + 0x100000), logical CPU 1 frame 0; all 12 found |
| Unit: QEMU-like region (4 frames, 128 KiB, Aff0 = CPU, Last on frame 3), identity map | frame n at base + n x 0x20000 |
| Unit: a CPU whose MPIDR no frame has | not found (0); the others found |
| Unit: a frame placed after the one with Last, matching a CPU | not used (the walk stopped) |
| All host tests, read-back tests | pass |
| CI QEMU checks (`qemu-direct-check.sh`, `partition-b-check.py`, cortex-a72 and max) | PASS, as before: same frames as the old computation in QEMU |
| Linux on two vCPUs (run V2 of `../20261108-multi-vcpu/`, at the D2 commit) | read-back matches, as before |
| i.MX95 and i.MX8QM builds | compile |
| Trusted code | about +40 code lines (estimate) |
