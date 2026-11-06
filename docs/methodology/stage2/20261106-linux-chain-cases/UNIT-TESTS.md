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
