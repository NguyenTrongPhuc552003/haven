# S2-R40 repeated before and after the redistributor fixes (design)

**Status:** design and expectations, 2026-11-24, before the runs.
Owner-approved ("Approve", 2026-11-24).

## Question [F]

S2-R40 (`take_requests` starts `left` at 1, so a virtual SGI is
re-queued on every pass) was found by M2 and M6 in scenario 2 on
2026-11-22 (run 44989942726, and in C4's rerun of the same case set) and
passed both on 2026-11-23 (run 45052511321, at `25be49b`). Between the
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
