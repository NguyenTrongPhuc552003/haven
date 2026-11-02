# A1 case set v3

**Status:** owner-approved 2026-11-02 (all 11 expected verdicts Y, grant
approved), frozen by `freeze/SHA256SUMS-v3` before the run. Held-out cases:
the owner asked for help; see `HELDOUT-GUIDE.md`. Claude drafted the cases.

## Why a scored set

The stage 2 runs so far (GIC feasibility, G2, hardening, interrupt delivery)
were written and judged by Claude, and several cases were tried before
their expected outcome was written down. They show the method works; they
are not evidence a thesis committee will accept, because the person who
built the tool also chose the faults. A scored set fixes that:

1. **Frozen inputs.** The tool, runner, grant and this file are hashed
   (SHA-256) before the run, so nothing changes after seeing results.
2. **Owner-approved verdicts.** You mark each expected read-back verdict
   below as right or wrong before the run.
3. **Held-out cases.** You write 2 to 4 faults of your own that Claude never
   sees before the run (see the last section). They test whether read-back
   finds drift that its author did not anticipate.
4. **All three checks scored on every case:** read-back, host unit tests,
   QEMU boot check, on cortex-a72 and max.

## Method

HAVEN at the commit that freezes this set; read-back v3 (stage-2, SMMU, GIC,
DMA); grant `grant-v3.json` (the current configuration: partition A RAM,
UART, read-only region, PCI config page read-only and BAR, DMA window
`0x5F000000` 1 MiB RW for SID 8, SPI 33 to A on CPU 0, SPI 106 to HAVEN);
QEMU 11.1.3 with `virtio-rng-pci,disable-legacy=on,iommu_platform=on`;
memory read after partition A prints "IRQ test done" or after 20 s.

None of the cases below has been run before, so all expected verdicts are
blind.

| Case | Point   | Seeded change                                                           | Expected read-back verdict (owner: Y / N / ?)                                              | Unit tests (predicted) | Boot check (predicted) |
| ---- | ------- | ----------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ | ---------------------- | ---------------------- |
| M3   | stage-2 | RWX regions get execute-never (`s2_attrs.h`)                            | execute drift on A RAM and B RAM                                                           | fail                   | fail (A cannot fetch)  |
| M4   | stage-2 | Device regions mapped as Normal write-back                              | memory-type and shareability drift on A UART, A PCI config, A PCI BAR                      | fail                   | pass                   |
| M5   | stage-2 | Partition B RAM 2 MiB larger (`memory.h`)                               | extent drift on B RAM                                                                      | pass                   | pass                   |
| S8   | SMMU    | DMA window configured read-only for SID 8                               | DMA access drift on SID 8                                                                  | pass                   | pass                   |
| S9   | SMMU    | every DMA window tagged with partition A's VMID (1)                     | no finding (read-back does not check VMIDs: a known blind spot)                            | pass                   | pass                   |
| I7   | GIC     | SPI 33 priority equal to HAVEN's priority mask (`0xF0`)                 | no finding (read-back does not read priorities or the CPU interface: blind spot)           | pass                   | pass                   |
| I8   | GIC     | Group 1 left disabled in GICD_CTLR                                      | GIC distributor drift                                                                      | pass                   | pass                   |
| P1   | DMA     | HAVEN places the BAR 16 KiB higher than granted                         | no finding (the stage-2 map is unchanged and read-back does not read PCI BARs: blind spot) | pass                   | pass                   |
| P2   | DMA     | the device's BAR is not mapped to partition A                           | missing binding on A PCI BAR                                                               | pass                   | pass                   |
| C6   | control | command-queue index computed with a different but equivalent expression | matches the grant                                                                          | pass                   | pass                   |
| C7   | control | stage-2 attribute lookup reordered, same descriptor bits                | matches the grant                                                                          | pass                   | pass                   |

S9, I7 and P1 are deliberate: they record what end-state read-back of the
current tool cannot see, so the thesis can state its limits with evidence
instead of by assumption.

## What the owner reviews

1. Each expected verdict: Y, N (and what it should be), or ?. - > I chose to mark all of them as `Y` (yes, the expected verdict is correct) for this proposal.
2. `grant-v3.json` (written at freeze time from the current configuration):
   is it the grant you intend? -> Yes, it is the grant I intend to use for this case set.
3. Held-out cases, below. -> I don't know what the held-out cases will be yet, can you help me kindly with that?

## Held-out cases (written by the owner, not shown to Claude)

Write 2 to 4 faults as small patches against HAVEN, each with the verdict
you expect from read-back, unit tests and the boot check. Any enforcement
point; a fault you think read-back will miss is just as useful.

To keep them unseen: put them in a folder outside the repository (for
example `~/thesis-heldout/`), one `NAME.patch` and one `NAME.expect.txt`
each, and run `run_cases_v3.py --held-out ~/thesis-heldout` yourself after
the main set; Claude will add that option when the set is frozen. Commit the
patches and their results into the case-set folder only after the run, so
the record shows they existed before it.

After approval: freeze with SHA-256 sums, run, score, report.
