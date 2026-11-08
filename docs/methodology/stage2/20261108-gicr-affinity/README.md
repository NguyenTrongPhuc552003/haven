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

## Results (2026-11-08)

HAVEN `93cf892` (expectations in `d520ec6`). Host tests on macOS arm64,
QEMU 11.1.3 for the rest. Each run is the first of its kind.

| Check | Expected | Observed | Verdict |
| - | - | - | - |
| CIX-like region | logical 0 gets frame 4, logical 1 frame 0, all 12 | `[PASS] CIX-like: 256 KiB frames, boot core 4 found by affinity` | as expected |
| QEMU-like region | frame n at n x 128 KiB | `[PASS] QEMU-like` | as expected |
| Missing CPU, frame after Last | not found; not used | `[PASS] missing CPU not found; walk stops at Last; Aff3 used` (Aff3 case added beside) | as expected |
| Host and read-back tests | pass | 26 of 26 host tests (one new), 42 read-back tests | as expected |
| CI QEMU checks | PASS | `qemu-direct-check` and `partition-b-check` PASS on cortex-a72 and max | as expected |
| Linux on two vCPUs (V2) | read-back matches | matches; CPU table 1/1, 1/1, 2/2, 0/0; `smp: Brought up 1 node, 2 CPUs`; no "redistributors found for" line (all CPUs found) | as expected |
| i.MX95, i.MX8QM builds | compile | compile | as expected |
| Trusted code | about +40 | 7,381 at `261f52d`, 7,443 at `93cf892`: +62 (two more files) | measured |

All 8 judged rows held. Not test-first this time: the unit test and
`gicr_frames.c` were written in the same step, so the test was never
seen failing before the code existed (method note, not a result).

On the i.MX boards the change is untested beyond compiling: there, the
frames are now found by the affinity in the platform's CPU map, which
must match the hardware's GICR_TYPER (it did not need to before).
