# A1 case set v3: results (scored)

**Status:** EXPERIMENT, scored 2026-11-02 at haven `4174daf`. Cases and
expected verdicts: `CASES-v3.md`, owner-approved (all Y) and frozen before
the run (`freeze/SHA256SUMS-v3`, checked OK after the run). Runner:
`run_cases_v3.py`; results: `runs/results.md`, `runs/results.json`;
per-case logs and diffs in `runs/<case>/`; memory dumps, and the two P2 UART
logs (11 MB each, cut to excerpts), replaced by `runs/DUMP_SHA256SUMS`.
Held-out cases: not yet written (see `HELDOUT-GUIDE.md`).

## Scores (both CPUs identical, 22 runs)

| Case | Seeded change | Expected (approved) | Read-back | Unit tests | Boot check | Only read-back |
| - | - | - | - | - | - | - |
| M3 | RWX regions execute-never | execute drift, A and B RAM | as expected | fail | fail | no |
| M4 | device regions Normal WB | memory-type and shareability drift on 3 regions | as expected | fail | pass | no |
| M5 | partition B RAM 2 MiB larger | extent drift, B RAM | as expected, plus association drift on A RO | pass | pass | **yes** |
| S8 | DMA window read-only | DMA access drift, SID 8 | as expected | pass | pass | **yes** |
| S9 | every DMA window tagged with VMID 1 | no finding (blind spot) | **not as expected**: DMA enforcement drift, SID 8 | fail | pass | (see below) |
| I7 | SPI priority at the priority mask | no finding (blind spot) | as expected: no finding | pass | pass | blind spot confirmed |
| I8 | Group 1 disabled in GICD_CTLR | GIC distributor drift | as expected | pass | pass | **yes** |
| P1 | BAR placed 16 KiB higher | no finding (blind spot) | as expected: no finding | pass | pass | blind spot confirmed |
| P2 | BAR not mapped to partition A | missing binding, A PCI BAR | as expected | pass | pass | **yes** |
| C6 | equivalent command-queue index | matches the grant | as expected | pass | pass | control |
| C7 | equivalent attribute lookup | matches the grant | as expected | pass | pass | control |

Of 8 cases where a finding was expected, read-back gave it in all 8; in 4
of them (M5, S8, I8, P2) neither the unit tests nor the boot check failed.
Both controls stayed clean. Two of the three predicted blind spots (I7, P1)
were confirmed: partition A reported "UART interrupt NOT delivered" and
"DMA device not usable" while read-back matched the grant. The third
prediction (S9) was wrong.

## S9: the prediction was wrong, and the reason is a HAVEN defect (SM4)

The case was meant to show that read-back does not check VMIDs. Instead
the stream table entry for SID 8 read back with S2TTB `0x1000080010000`:
the VMID (1) had leaked into the table address, the SMMU rejected the entry
(4 "bad STE" events, logged by HAVEN and seen by read-back), and partition A
reported "DMA inside window FAILED". The unit-test failure in S9 comes
from the mutant's VMID allocator, not from this defect.

Cause (source-derived): `smmu_v3_set_ste_partition()` takes a VTTBR-format
value (VMID in bits [63:48]) and stores `vttbr & STE_3_S2TTB_MASK`, whose
mask covers bits [51:4]. VMID bits [51:48] therefore end up in the table
address. HAVEN's own VMIDs start at `0xC0`, which has those bits clear, so
the first window works; the second (`0xC1`) has bit 48 set and would be
rejected exactly as in S9 (only `0xC0`, `0xD0`, `0xE0` and `0xF0` are safe). Recorded as SM4 in the finding ledger; fix
proposed, not applied.

## Observations

- M5's extra finding is a correct consequence of the change: the read-only
  region's address is derived from partition B's size, so it moved too.
- P2: with its BAR unmapped, partition A's driver spins on a faulting read;
  HAVEN logs every stage-2 denial, 143,139 lines in 20 s. An unbounded log
  is a denial-of-service path for the console (observation; no ledger
  entry yet).

## Threats to validity

Claude wrote the cases, the tool and the runner; the owner approved the
verdicts but did not choose the faults. Held-out cases are still missing,
so this set does not yet test faults the author did not anticipate. One
platform (QEMU virt, single security state), one device per bus.
