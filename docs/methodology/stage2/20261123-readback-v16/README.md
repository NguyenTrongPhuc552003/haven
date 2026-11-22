# Read-back v16: every granted stream checked through its own table entry (design)

**Status:** design and expectations, 2026-11-23, before any code.
Owner-approved ("Approve", 2026-11-22: read-back v16, design first).

## Problem [F]

Read-back finds a granted DMA stream that is not translated, or is
translated by the wrong tables, only through its list of DMA masters,
which comes from the QEMU monitor (`dma_masters`, `query-pci`, `info
qtree`). The EL3 snapshot has no monitor, so in `--el3` mode the list is
empty and the check never runs (`../20261119-evaluation-campaign/m7/`,
W14 and R60). The stream-table loop of `compare_smmu`, which runs in both
modes, skips entries that abort and compares a translated entry with its
window only when its table resolves to a partition. So in EL3 mode:

- W14 and R60 (stream 8's entry left at abort) pass M7;
- H2 (stream 255's table address carries the VMID's low bit, scenario 3)
  passes M7, because that table resolves to no partition.

## Design [P]

In `compare_smmu`'s stream-table loop, for each **granted** stream that
no DMA master in the list covers:

1. entry aborts or is invalid: in EL3 mode (no device list,
   `snap["pci"]` empty) a finding, `granted stream not translated`; in
   monitor mode (the device list is known and has no device on that
   stream) an observation in the report, `withheld: no device`, which is
   what HAVEN does when a granted device is absent;
2. entry translates through a table that resolves to no partition: a
   finding, `DMA enforcement drift` (`translated:unknown`, expected the
   granted partition), the same name the master path uses.

Streams a master covers are left to the master path, so monitor-mode
findings and their evidence do not change. The version line and
`Changes in v16` go into `readback.py`'s header.

## Expectations

| Row | Check | Expected |
| - | - | - |
| V1 | new unit tests (`tools/readback/test_smmu_granted.py`) | EL3 mode: abort entry on a granted stream is a finding; monitor mode, no device: an observation, no finding; a master covering the stream: no added finding; a translated entry whose table is outside the image: `DMA enforcement drift` |
| V2 | campaign scenario 1, M6 | every verdict and evidence line as in the run after Amendment 1 of `20261122-firmware-redistributor` |
| V3 | campaign scenario 1, M7 | the controls pass; W14 and R60 found (`granted stream not translated`, SID 8) |
| V4 | M7 and M6 on scenario 1 | agree on **102 of 106**: the four left are W09 and R42 (per-CPU VTTBR_EL2) and W27 and R19 (groups under TF-A) |
| V5 | scenario 3, M7 | H2 found (stream 255); every other verdict as in run 44999284558 |
| V6 | other read-back users (the Linux CI job, the R1 demo, the UEFI checks) | unchanged: they read in monitor mode with their devices present |
