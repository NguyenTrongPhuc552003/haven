# Follow-up: do the host unit tests catch the five invisible faults?

**Status:** expectations written by Claude on 2026-11-07, before the run
(owner approval: "Approve", item 3 of the follow-ups). Not part of the
frozen case set: the same patches (`patches/H1.patch`, `H3.patch`,
`heldout/R1-R3.patch`), a different instrument. Results are appended
below after the run.

Method: for each patch, `git archive` of haven `0035b62`, apply the
patch, `cmake --preset host-tests && cmake --build build-host && ctest`
(all host tests, `bench_temporal_isolation` excluded as in CI).

## Expectations (from reading `tests/unit/test_vgic.c`)

| Fault | Expected | Why |
| - | - | - |
| H1 (model enables any interrupt) | `test_vgic` fails | the test's hardware hook asserts every enable is for an owned interrupt (line 24), and the test enables a SPI the partition does not own |
| H3 (owned interrupts left enabled at start) | `test_vgic` fails | it asserts two disable calls at start (line 55) |
| R1 (group range check never rejects) | `test_vgic` fails | it reads the redistributor's SPI group word and expects 0 (line 135); with R1 that read returns the distributor's stored value |
| R2 (affinity mask) | all pass | the test uses MPIDR 0x80000000, whose affinity is 0 with either mask |
| R3 (trap log cap) | all pass | no host test exercises the trapped system register logger |

If these hold, unit tests catch 3 of the 5 faults that no end-state check
saw, and R2 and R3 are untested by both instruments.

## Results (2026-11-07, macOS, haven `0035b62` plus each patch)

| Fault | Host tests | Expectation |
| - | - | - |
| H1 | 23 of 24 pass; `test_vgic` fails after "probe sequence" and "distributor init writes" (in the enable test) | held |
| H3 | 23 of 24 pass; `test_vgic` fails at line 55 (`hw_calls == 2 ...`) | held |
| R1 | 23 of 24 pass; `test_vgic` fails after "distributor init writes" (in the enable test) | held |
| R2 | 24 of 24 pass | held |
| R3 | 24 of 24 pass | held |

5 of 5 expectations held. Together with the case set: of the 15 faults,
**13 are caught by at least one instrument** (demand check, read-back,
boot or HAVEN log, host unit tests), and **R2 and R3 by none**. R2 needs a
vCPU with a non-zero affinity (a second vCPU, or a unit test with such an
MPIDR); R3 needs a test of the trap logger. Both are cheap unit tests to
add; they are listed as follow-ups, not added here, so this record shows
the gap as it was.
