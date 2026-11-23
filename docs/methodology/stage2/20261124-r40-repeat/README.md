# S2-R40 repeated before and after the redistributor fixes (design)

**Status:** design and expectations, 2026-11-24, before the runs.
Owner-approved ("Approve", 2026-11-24). Results below, 2026-11-24.

## Question [F]

S2-R40 (`take_requests` starts `left` at 1, so a virtual SGI is
re-queued on every pass) was found by M2 and M6 in scenario 2 on
2026-11-22 (S2b's run, and C4's run 44989942726 of the same case set) and
passed both on 2026-11-23 (run 45052511321, at `9262504`; corrected
from `25be49b`, which is scenario 1's run 45052417129). Between the
runs HAVEN changed how a started CPU sets up its redistributor (#202,
#206); read-back v16 and v17 do not touch the path, and KDAL's pin moved
with an unchanged module source (#210). Does the change come from #202
and #206, or from timing?

## Method [P]

`tools/campaign/cases/r40-repeat.json`: scenario 2's control once, then
S2-R40 three times, as one campaign run (scenario `linux2`, four shards)
at each of:

- `main` after this record;
- `c2ca532` (the merge of #201, the last commit before #202), through a
  branch `r40-before-202` that adds only this cases file to it (its
  lock still pins KDAL at `5622c47`).

## Expectations

| Row | Check | Expected |
| - | - | - |
| Q1 | the control, both commits | passes every method |
| Q2 | if #202 and #206 caused the change | S2-R40: 3 of 3 found by M2 at `c2ca532`, 3 of 3 pass at `main` |
| Q3 | if timing did | a mix at either commit |

Either outcome is recorded; a mix (Q3) makes R40 a timing-dependent case
of scenario 2 whose verdict is not counted as a detection or a miss.

## Results (2026-11-24) [F]

Two runs, both campaign `linux2`, four shards: run 45229962365 at
`3e75496` (`c2ca532` plus the cases file) and run 45229938238 at
`784ffd0` (`main` after this record). M4 and M7 are n/a throughout.

| Case | `c2ca532` (before #202) | `main` |
| - | - | - |
| S2-BASE-1 | every method passes | every method passes |
| S2-R40-a | M2 F, M5 error, M6 F (PPI 27 on CPUs 0 and 1) | M2 F, M5 error, M6 F (PPI 27 on CPU 0) |
| S2-R40-b | M2 F, M5 error, M6 F (PPI 27 on CPU 0) | M2 F, M5 error, **M6 pass** |
| S2-R40-c | M2 F, M5 error, M6 F (PPI 27 on CPU 0) | M2 F, M5 error, M6 F (PPI 27 on CPU 1) |

M2's evidence is the same in all six: no `KDAL-DEMAND-END`, no power-off,
no hotplug lines (Linux did not reach the demand). M5's error is "no
demand printed (the boot did not reach it)". M1 and M3 pass.

- **Q1 holds**: the control passes every method at both commits.
- **Q2 does not hold**: M2 finds R40 in 6 of 6, at `main` as well as
  before #202.
- **Q3 holds**: M6 is a mix at `main` (2 of 3 found, which CPU's PPI 27
  is missing also varies).

**The pass of 2026-11-23 is not reproduced, and the code was the same
[F].** `git diff 9262504 784ffd0` touches only two records and the cases
file; nothing under `src/`, the scripts or the lock changed. So the one
run where R40 passed every method ran the same HAVEN that now fails M2
three times out of three. Over every scenario-2 run of R40 on record
(scenario 2's first run, S2b's rerun at `9e19bb2`, C4's run 44989942726,
45052511321 and these six), M2 finds it in 9 of 10 and M6 in 7 of 9 (the
first run's M6 is left out: every case carried the same three DMA
findings there, `s2/README.md`).

**Reading [I].** R40 floods the vCPU with re-queued SGIs. Whether Linux
gets through its boot depends on whether its timer PPI finds a free list
register in time; most runs it does not, and Linux stalls before the
demand. Read-back's snapshot then catches PPI 27 without its binding on
whichever CPU was starved at that moment, which is why M6's evidence
moves between CPU 0 and CPU 1 and once finds nothing. Neither #202 nor
#206 is implicated.

**Consequence, as fixed in the expectations.** Q3 makes R40 a
timing-dependent case of scenario 2: its verdict is counted neither as a
detection nor as a miss for any method. Scenario 2's random cases then
count 39: M2 finds 4 (R4, R7, R39, R53) and M6 finds 1 (R39). Run
44989942726's own figures stay as recorded; `RESULTS.md` carries the
note.

**Process slip, mine.** The two runs were first dispatched before #217
merged: run 45228663439 failed (no cases file on `main` at `5a13429`)
and run 45228684451 was cancelled. Both were dispatched again after the
merge; only the runs above are counted.
