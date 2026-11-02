# Interference pipeline in QEMU: run of 2026-11-03

**What this is:** a check that the measurement pipeline in `PLAN.md`
works end to end (prerequisites 1 to 3 in QEMU). It is not an
interference measurement. QEMU's TCG models no caches, no memory
bandwidth and no interconnect, and its timing is the Mac's (see
`PLAN.md`, "Why QEMU cannot answer it").

- Haven commit: `fe7db3f`, built per cell by `run_pipeline.py`.
- Command: `python3 run_pipeline.py <haven root> runs` (about 25 min on
  the Mac).
- Raw output: `runs/<config>/` (build log and the partition B check for
  each CPU model), `runs/results.json`, table in `runs/results.md`.

## Configurations

| Config | Partition A load (`HAVEN_DEMO_A_LOAD`) | Partition B budget |
| - | - | - |
| none | 0 (idle after its tests) | 2 ms per 10 ms |
| cpu | 1 (CPU loop) | 2 ms per 10 ms |
| mem | 2 (stream over 64 MiB at IPA 0x48000000) | 2 ms per 10 ms |
| none-B-unthrottled | 0 | 10 ms per 10 ms (no hold) |

Each cell ran once with `-cpu cortex-a72` and once with `-cpu max`.

## Pipeline checks (observations)

A cell counts as ok when all of these hold in its UART log and status
record: the partition B check passes, partition A prints "caches on", the
load line appears when a load is selected, partition B records 500
latency samples, and partition B's SCTLR_EL1 has M and C set.

**All 8 cells were ok.** So, in QEMU:

1. both demo guests run with their stage-1 MMU and data cache on (the
   PLAN's prerequisite 1);
2. partition B's deadline-to-handler probe fills its histogram and the
   host-side check reads it back through the status record
   (prerequisite 2);
3. partition A's CPU and memory loads build and run (prerequisite 3);
4. isolation still holds with caches on: the stage-2 denials in both
   directions and the read-back checks behave as before.

## The latency numbers (reported, not interpreted)

| Config | CPU | avg / max (us) | < 1 ms | 1 to 10 ms |
| - | - | - | - | - |
| none | cortex-a72 | 3990 / 8970 | 247 | 253 |
| none | max | 4007 / 9068 | 246 | 254 |
| cpu | cortex-a72 | 3973 / 9278 | 248 | 252 |
| cpu | max | 5111 / 9260 | 192 | 308 |
| mem | cortex-a72 | 5361 / 9154 | 177 | 323 |
| mem | max | 5348 / 9050 | 181 | 319 |
| none-B-unthrottled | cortex-a72 | 526 / 2385 | 497 | 3 |
| none-B-unthrottled | max | 522 / 2025 | 496 | 4 |

Observation: with partition B's own budget removed, the average falls
from about 4 ms to about 0.5 ms.

Inference (labelled as such): with the budget, a timer interrupt that
falls in the 8 ms hold of each 10 ms period waits for the next period, so
these numbers mostly measure partition B's own budget plus the host's
scheduling of QEMU threads. The higher averages in the `mem` cells and in
`cpu`/`max` are not evidence of hardware interference: TCG does not model
the resources an aggressor would share, so the plausible cause is host
CPU contention between QEMU's vCPU threads. One run per cell; no
statistics are drawn.

## What this leaves for the board

The board run follows `PLAN.md`'s experiment matrix: the same probe and
loads, with victim placement on A720 and A520 cores, at least 10 runs
per cell and 10^5 samples each. Two changes the QEMU run suggests:

- run the victim with its budget off (or long enough) when the question
  is interference, so the probe does not mostly measure its own hold;
- raise `PROBE_IRQS` well above 500 for percentiles beyond p99.
