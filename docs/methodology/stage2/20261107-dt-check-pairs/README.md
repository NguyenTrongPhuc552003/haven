# Second blind test of dt_check.py: two faults per tree

**Status:** owner-approved 2026-11-07 ("Approve", item 2). Design by
Claude, frozen (`freeze/SHA256SUMS`) and committed before the draw; the
draw and Claude's predictions follow in a later commit, then the runs.

## Question

The single-fault test (`../20261107-dt-check-blind/RESULTS.md`: 5 true
positives, 7 true negatives) changed one thing per tree. Do two faults in
one tree interact: does one hide the other from dt_check, or make
dt_check report something neither would alone?

## Faults and labels

The frozen catalogue of the single-fault test, unchanged (41 labelled
faults, same grant `grant-l4.json`, same specification). Operators that
edit the same part of the tree would overwrite each other, so they are
grouped by what they edit (memory node; initramfs range; KDAL node;
timer node; CPU nodes; extra node) and a pair takes two different
groups. All 627 possible pairs compile with dtc (checked before the
freeze; dt_check was not run on them). A pair is labelled outside the
grant if either fault is.

## Draw

12 pairs. For each: two different groups, uniform without replacement,
then an operator uniform within each group, then a parameter uniform
within the operator, from `random.Random` seeded with the SHA-256 of
`haven <commit> | owner 2026-11-07 02:31: 1. Approve 2. Approve 3.
Approve, we have our own data/ directory for your discovery and
investigation about our SBC's specifications.`, where `<commit>` is the
short hash of the commit that freezes this record.

## Measures

- dt_check against the pair labels (true and false positives and
  negatives), and whether each reported finding is exactly the union of
  what each fault alone produces (from the single-fault catalogue's
  stated findings).
- Boot under HAVEN, as in the single-fault test. P00 is the unchanged T1
  tree (control).

## Threats to validity

As in the single-fault test, plus: grouping rules out pairs that edit the
same node, which are the pairs most likely to interact; this test covers
interactions between different parts of the tree only.
