# Third blind test of dt_check.py: two faults in the same node

**Status:** owner-approved 2026-11-07 ("Approve", item 2 of 21:17). Design
by Claude, frozen (`freeze/SHA256SUMS`) and committed before the draw.
After the draw and before any run, Claude writes the labels
(`labels.json`) and predictions (`EXPECT.md`) in one commit; then the runs.

## Question

The pair test (`../20261107-dt-check-pairs/`) kept its two faults in
different nodes and found that dt_check's findings compose. Faults in the
same node can interact: two changes that are each outside the grant can
together be inside it (a 2-page register range moved into a 4-page device
region), and two that are each inside can together be outside (a register
base on the last granted page combined with a larger size). Does
dt_check judge the merged node, not the parts?

## Combinations

`dt_same.py`: 69 combinations of the frozen single-fault catalogue's
values, merged field by field into one node:

| Node | Combination | Count |
| - | - | - |
| KDAL device node | register base (O5) with size (O4) | 20 |
| KDAL device node | register base (O5) with interrupt (O6) | 20 |
| KDAL device node | size (O4) with interrupt (O6) | 16 |
| memory node | changed base (O2: 0x40100000 or 0x3FF00000) with size (O1) | 10 |
| timer node | secure entry PPI 9 with virtual PPI 10, 12 or 15 | 3 |

All 69 compile with dtc (checked before the freeze; dt_check was not run on
them). The grant and specification are those of the single-fault test
(`grant-l4.json`; `../20261107-dt-check-blind/README.md`).

## Labels

Not attached to the catalogue: each drawn combination is labelled by hand
from `grant-l4.json` and the specification, with the arithmetic written
out, after the draw and before dt_check runs. The single-fault labels of
its parts are listed next to it, so the cases where the merged label
differs from "either part outside" are visible.

## Draw

12 of the 69, uniformly without replacement, `random.Random` seeded with
the SHA-256 of `haven <commit> | owner 2026-11-07 21:17: 2. Approve`,
where `<commit>` is the short hash of the commit that freezes this record.

## Measures and threats

As in the earlier blind tests: true and false positives and negatives
against the labels, and a boot of each tree under HAVEN (S00, the
unchanged T1 tree, as control). The labels and dt_check share an author;
the draw removes the choice of which combinations run, not which exist.
