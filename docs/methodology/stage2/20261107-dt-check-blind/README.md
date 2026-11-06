# Blind test of dt_check.py

**Status:** owner-approved 2026-11-07 ("Approve", item 2). Design, fault
catalogue and labels by Claude, frozen (`freeze/SHA256SUMS`) and
committed before the faults are drawn. The draw, then the runs, follow in
later commits.

## Question

`tools/readback/dt_check.py` was written knowing which case-set trees
failed (`../20261107-readback-v8/RESULTS.md`, section 3). Does it report
device-tree faults it has not seen, and only those?

## Specification the labels follow

From the v8 results record, not from the code: a partition's tree is
outside its grant if an enabled node uses
1. memory or an initramfs range not inside granted Normal RAM;
2. device registers not inside a granted device region (`DEVICE_*`) or a
   range HAVEN emulates (here the GICD and GICR frames);
3. a SPI not owned by the partition, or a PPI not granted on its CPUs
   (for `arm,armv8-timer`, only the virtual timer entry);
4. more CPU nodes than the partition has CPUs in `gic.cpus`.

Disabled nodes are ignored.

## Faults

`dt_faults.py` holds 10 operators with 41 parameters, each labelled by
hand from `grant-l4.json` and the specification above (23 outside the
grant, 18 inside), with a reason per label. Every parameter changes the
T1 tree (the case set's matching tree) and compiles with dtc; dt_check
was not run on any of them. The operators: memory size; memory base and
size; initramfs range; KDAL node register size; KDAL node register base;
KDAL node interrupt; timer entries; extra CPU nodes; extra device node;
neutral edits (reorder, rename, bootargs, node removed).

Boundary cases are included on purpose: a range ending exactly at the end
of RAM, the last page of the PCI BAR, a node in the read-only Normal
region `A RO`, a second CPU node (partition 1 has CPUs 0 and 1 in
`gic.cpus`), a disabled node, a change to a timer entry that is not the
virtual one.

## Draw

12 faults, drawn with replacement: operator uniform over the 10, then
parameter uniform within it, from Python's `random.Random` seeded with
the SHA-256 of the text `haven <commit> | owner 2026-11-07 01:51: 1.
Approve 2. Approve 3. Approve 4. I had already deleted it on Github
permanently`, where `<commit>` is the short hash of the commit that
freezes this record. The draw is committed with Claude's boot predictions
before dt_check or any boot runs.

## Measures

- dt_check against the labels: true and false positives and negatives
  over the 12 faults (exit 1 = reported, 0 = matches). M00, the unchanged
  T1 tree, is a control and not scored.
- Each tree is also booted under HAVEN (`run_dt_blind.py`, same Linux,
  KDAL and initramfs as the case set, 45 s limit): does a reported tree
  break the boot, and does an unreported one boot?

## Threats to validity

- The labeller and dt_check were both written by Claude from the same
  specification; the test checks that the implementation follows the
  specification on unseen inputs, not that the specification is the
  right one.
- The catalogue was written after dt_check, by someone who knows its
  rules. The draw removes the choice of which faults are run, not the
  choice of which faults exist.
- One fault per tree; combinations are not tested.
