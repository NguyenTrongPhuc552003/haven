# Read-back v19: HAVEN's interrupt ownership table against the grant (design)

**Status:** design and expectations, 2026-11-28, before any code.
Owner-approved ("Approve", 2026-11-28).

## Why

N3-W1 (`../20261128-readback-n3/`) took vCPU 2's timer PPI away and
read-back passed it: read-back v8 takes "did the guest enable it?" from
HAVEN's GIC model, and the model drops an enable of an interrupt HAVEN
does not think the partition owns (`src/core/irq/vgic.c`, `set_enable`).
The fault hides behind HAVEN's ownership table, the state it corrupts
[F]. Read-back already reads HAVEN's own state where the hardware cannot
say enough (the GIC model, v8; the vCPU table, v12; `cpu_partition`,
v10). The ownership table is the same kind of state.

## The rule [P]

Read-back reads HAVEN's three ownership arrays from the image
(`src/core/irq/ownership.c`, static, found by symbol like
`cpu_partition`):

- `ppi_owner_map[16][16]`: per CPU, the owner of PPIs 16 to 31;
- `irq_owner_map[1024]` and `irq_target_cpu_map[1024]`: per SPI, the
  owner and target CPU.

Against the grant:

| Case | Finding |
| - | - |
| a PPI granted to the partition on CPU c (`ppis` or `guest_enabled`) whose table entry is not that partition | "missing interrupt binding" (`CPU c PPI n: HAVEN's table holds partition X`) |
| a table entry giving a partition a PPI the grant does not give it on that CPU | "interrupt ownership drift" |
| a granted SPI whose table owner or target CPU differs from the grant | "missing interrupt binding" or "interrupt routing drift" |
| a table entry giving a partition an SPI the grant does not | "interrupt ownership drift" |

HAVEN's own interrupts (`haven_ppis`, `maintenance_ppi`, SPIs granted to
partition 0) are not in the partition tables and are not checked here.
The EL3 path (M7) dumps HAVEN's image as well, so the rule runs there
too. An image without the symbols (an older build) skips it with an
observation.

## Expectations

| Row | Check | Expected |
| - | - | - |
| V1 | read-back unit tests | a table that matches: no finding; each row of the table above gives its finding; a missing symbol: an observation, no finding; all earlier tests pass |
| V2 | the controls: scenario 1 (six), scenario 2 (three), `linux3` (three) | every method passes, M6 and (scenario 1) M7 included (no false finding from the new rule) |
| V3 | N3-W1 again | M6 finds it: `CPU 3 PPI 27` missing interrupt binding, HAVEN's table holding no owner |
| V4 | the campaign's case sets again with v19 (scenario 1, 106 cases; scenario 2 without S2-R38, 42) | every M6 verdict that changes from v18 is listed and explained; the frozen campaign's figures stay as recorded, v19's are reported beside them [?: no prediction of the count] |
| V5 | read-back's time | the M6 median rises by less than 1 s |

## Results (2026-11-28) [F]

Code on pull request #233 (branch `feat/readback-v19`); the runs are
at `d091e36` (the code, `f46ec81`, and a cases file). Campaign runs: 46130034849
(scenario 1's six controls), 46130037381 (scenario 2's three),
46130040417 (`linux3`, `n3.json`), 46130043081 (scenario 1, 103 cases)
and 46130046780 (scenario 2, 42 cases).

| Row | Result |
| - | - |
| V1 | 8 new tests (`test_ownership.py`): a matching table, a missing PPI route (N3-W1's), a PPI held for the wrong partition, a PPI the grant does not give, an SPI with the wrong owner, the wrong target, an extra SPI, a missing SPI, HAVEN's own interrupts left out, reading the arrays from an image, an image without them. All 91 read-back tests pass. **Holds** |
| V2 | all twelve controls pass every method: scenario 1's six with M6 and M7 (EL3), scenario 2's three, `linux3`'s three. Read-back reports HAVEN's table on every run (for example CPUs 0 to 3 holding PPI 27 for their partitions, SPI 33 to partition 1 on CPU 0). **Holds** |
| V3 | N3-W1: M6 now finds it, `CPU 3 PPI 27: missing interrupt binding (HAVEN's ownership table holds no owner, granted partition 1)`. **Holds** |
| V4 | Scenario 1 against the v17 run 45052417129 (at `25be49b`): 102 of 103 cases give the same verdict from every method. **C2-R41 changes: M6 and M7 from pass to finding.** R41 starts HAVEN's owner map with every interrupt owned by partition 1 (`irq_owner_map[i] = 1U`); v19 reports every SPI the grant does not give as "interrupt ownership drift". The campaign had R41 as found by M1 alone in scenario 1 (`../20261119-evaluation-campaign/c2/random/`); its open question "why read-back passes R41" now has its answer: read-back did not read the table. Six other cases (R11, R13, R31, R37, W30; R32 through M7 only) also get v19 findings, but were findings already. Scenario 2 against the run before v19 (46122075748): 42 of 42 same verdicts; only S2-R39, a finding already, gains one evidence line. **Holds**: every change listed and explained |
| V5 | M6's median 0.8 s over 102 scenario-1 runs (v17's run: 1.0 s over 105). **Holds** |

The V4 comparison with the v17 run also spans the LR change (#227); none
of the 103 verdicts changed by it in scenario 1. Three scenario-1 cases
(C2-V3-P2, C2-R38, C3-H3) were left out because their patches no longer
apply after that change; their frozen verdicts stand. The frozen
campaign's figures stay as recorded; v19's are these.
