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

## Code and V1 (2026-11-23) [F]

`compare_smmu` as designed. V1: the four new tests pass (71 read-back
tests in all). **V1 holds.** A preview on the Mac (QEMU 11.1.3, the
local TF-A build, `--el3`): the unchanged tree matches the grant; W14
gives `granted stream not translated (SID 8: entry abort, granted to
partition 1)`; H2 on the grant-2 build gives `DMA enforcement drift (SID
255: translated:unknown (S2TTB 0x100008001a000, table outside the dumped
image), expected translated:1)`. V2 to V6 are judged on the campaign
runs after merge.

## Results (2026-11-23) [F]

Run 45047873482 (scenario 1, 106 cases) and run 45047886090 (scenario 3,
109 cases), both at `63de0a8`, the merge of #208.

| Row | Result |
| - | - |
| V2 | against A5's run 45047328899, every verdict and every evidence line of M1 to M6 is the same, except the order of quoted parallel build lines in H1's M4. **Holds** |
| V3 | the controls pass M7; W14 and R60 are found (`SMMU: granted stream not translated`). **Holds** |
| V4 | M7 and M6 agree on **102 of 106**; the four left are W09, R42, W27 and R19. **Holds** |
| V5 | H2 is found by M7 (`SMMU: DMA enforcement drift`). Against run 44999284558 three more M7 verdicts changed, each for a recorded reason: W14 and R60 (V3, here once per granted stream) and W30 (F5, the baseline rule retired). M7 and M6 agree on 105 of 109. **Holds** for H2; the row's "every other verdict" was written before #202 and did not allow for W30 |
| V6 | the Linux job and the other CI jobs passed on #208 |

With v16, every M7 difference from M6 that is left comes from what the
EL3 dump cannot hold (a CPU's own EL2 registers: W09, R42) or from a
fault that cannot happen under TF-A's two security states (W27, R19).
