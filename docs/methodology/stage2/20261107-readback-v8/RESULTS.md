# Read-back v8 and the device-tree check: results

**Date:** 2026-11-07. Expectations in `README.md` were committed in
`4101733` (00:57:11 +0700); the first run started at 00:57:28. Raw output:
`runs/V1..V6/` (UART log, device tree, read-back text and JSON) and
`runs/results.md`. The device-tree check below is **retrospective**: it was
written after the case set's results were known, so it is not a blind test.

## 1. Read-back v8 (follow-up 1)

| Run | Case | Expected | Observed | Expectation |
| - | - | - | - | - |
| V1 | B0 | matches; 27, 33 enabled by guest and GIC | matches; guest 1 / GIC 1 for both | held |
| V2 | T4 | matches; both waiting for the guest | matches; both waiting | held |
| V3 | T5 | matches; both waiting for the guest | matches; both waiting | held |
| V4 | H3 | matches (fault invisible once the guest has asked) | matches; guest 1 / GIC 1 for both | held |
| V5 | B0 early | matches; both waiting | matches; both waiting | held |
| V6 | H3 early | two drift findings: SPI 33 and CPU 0 PPI 27 | **one** drift finding (SPI 33); PPI 27 guest 1 / GIC 1 | **partly missed** |

**Observation (V6).** The read was triggered by `Booting Linux`, but the
UART log of V6 runs to `[0.088070] devtmpfs: initialized` (114 lines)
before QEMU stopped, while V5's stops after 21 lines. By then Linux had
initialised its timer and enabled PPI 27 itself, so the drift on 27 was
no longer visible. SPI 33 (KDAL's interrupt, enabled much later) was still
caught.

**Inference.** v8 turns the v7 false findings in T4 and T5 into correct
"waiting for the guest" lines, and makes H3 visible, but only inside the
window before the guest enables the interrupt itself. The read point after
a UART marker is not precise: the delay between the marker and the stop
varied from about 0 to about 90 ms of guest time in these two runs. An
H3-class fault on an interrupt the guest enables early can be missed.
Two ways to close the window (proposals, not done): stop the guest at a
breakpoint instead of a UART marker, or let HAVEN record the first moment
each owned interrupt became enabled and by whom.

## 2. Denied-access log rate limit (follow-up 3)

| | v7 T4 run (no limit) | V2 (limit: first 16, then every 4096th) |
| - | - | - |
| Denied accesses in 45 s | 101,905 (all logged) | 794,624 (`n=` of the last line) |
| Denial lines in the log | 101,905 | 210 (16 + 194 numbered) |
| UART log size | 15,851,904 bytes | 34,650 bytes |

Expected "at most 16 plus a few numbered lines": **missed** on the count.
The limit works as designed; the prediction underestimated the rate. With
no printing to slow it, the guest made 7.8 times more denied accesses
(about 17,700 per second), so one line every 4096 accesses is about four
lines per second. Expected "no total line, the partition does not stop":
held (no total line in V2).

**Inference.** The v7 log measured the UART, not the guest: printing every
denial throttled the faulting guest by a factor of about 8. Counts from
logs without the limit should not be used as fault rates.

## 3. Device tree against grant: `tools/readback/dt_check.py` (follow-up 2)

Checks, before boot, that every enabled node in a partition's device tree
uses only what the grant gives that partition: memory and initramfs in
granted RAM, device registers in granted device regions (or in ranges HAVEN
emulates, given with `--emulated`), GIC SPIs and PPIs owned by the
partition, and no more CPU nodes than the partition has. For
`arm,armv8-timer` only the virtual timer entry is checked; the others are
reported as information. Tests: `tools/readback/test_dt_check.py` (7,
skipped where `dtc` is missing).

Retrospective run on the frozen case set's device trees with
`grant-l4.json` (output: `dt-check-cases.txt`):

| Case | dt_check findings | Case set (v7 chain) outcome |
| - | - | - |
| B0 | KDAL GPIO registers, SPI 39 | the same gap, found by demand_check after boot |
| T1 | none | completes, nothing new |
| T2 | UART registers 0x9000000+0x2000 | found by demand_check after boot |
| T3 | KDAL GPIO registers | found by demand_check after boot |
| T4 | **memory 0x40000000+0x40000000** (plus the GPIO gap) | boot did not complete; demand_check and read-back saw nothing new |
| T5 | **PPI 28 not granted** (plus the GPIO gap) | boot did not complete; nothing pointed at the timer |
| C1, C2 | as B0 | as B0 |

**Observation.** The two cases the chain could not explain (T4 and T5,
which hold all 4 missed predictions of the case set) are each named by
one dt_check line before any boot.

**Limits.**
- T3 wires KDAL's GPIO node to the UART's SPI 1. SPI 1 is granted to the
  partition, so the tree is within the grant and dt_check is silent on the
  wiring error. It checks containment, not correctness.
- Not blind: the checks were chosen knowing which cases failed. A fair test
  needs new tree faults drawn before the tool sees them (as `heldout_random_linux.py`
  did for the HAVEN patches).

## 4. Score

7 expectations: 5 held, 2 missed (V6 in part; the V2 log count). Both
misses are recorded above with their cause; no expectation was edited after
the runs.
