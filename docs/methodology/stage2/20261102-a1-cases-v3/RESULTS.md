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
rejected exactly as in S9 (only `0xC0`, `0xD0`, `0xE0` and `0xF0` are
safe). Recorded as SM4 in the finding ledger and fixed in haven `2b6f5ea`
(`smmu_ste3_s2ttb()` drops VTTBR bits [63:48] before masking; unit test
`test_smmu_ste` covers VMIDs 0xC0-0xFF).

**S9 rerun on the fix** (haven `ce220bb`, `runs-after-sm4/S9/`): the
stream table entry reads back with S2TTB `0x80010000`, no "bad STE" event,
partition A reports "DMA inside window OK" and "DMA outside window
blocked". Read-back matches the grant and the boot check passes, so the
original prediction now holds: a DMA context sharing VMID 1 with
partition A is invisible to read-back (blind spot). The unit test still
fails, from the mutant's allocator.

## Randomly drawn held-out cases (R1-R4)

Four faults drawn at random from all 1,062 single-point mutants of the
enforcement code (`heldout_random.py`, seed and draw in
`heldout-random/draw.json`), expectations written by Claude before the run
and frozen in `freeze/SHA256SUMS-v3-random` (13 files, re-checked OK after
the run). Run at haven `ce220bb`; raw output in `runs/held-out/`, large
dumps listed in `runs/held-out/DUMP_SHA256SUMS`. Both CPUs gave identical
results. The expectations are Claude's, not the owner's.

| Case | Mutation | Read-back: expected / observed | Unit: exp / obs | Boot: exp / obs | Match |
| - | - | - | - | - | - |
| R1 | `stage2.c:115` size check `== 0U` to `== 1U` | none / none | pass / **fail** | pass / pass | 2 of 3 |
| R2 | `gic_v3.c:77` SPI priority loop starts at 9 | none / none | pass / pass | pass / pass | 3 of 3 |
| R3 | `smmu_v3.c:358` first init command uninitialised | SMMU findings / **tool crash, no report** | pass / pass | fail / fail | 2 of 3 |
| R4 | `partition.c:374` `!=` to `==` after mapping B | 3 finding kinds / all 3 present | pass / pass | fail / fail | 3 of 3 |

What each case showed (experimental observation unless marked):

- **R1.** The unit test `test_isolation_negative` submits a zero-size
  region and expects `HV_EINVAL` (assertion at line 111); the mutant accepts
  it. Claude's expectation hedged this ("low confidence") and was wrong. At
  run time the mutant changes nothing, as expected: read-back and the boot
  check agree.
- **R2.** No method detects it. INTIDs 32-35 keep reset priority 0 instead
  of 0xA0 (source-derived), so the UART interrupt owned by partition A
  outranks HAVEN's own priority 0x40 (inference: not measured, because
  read-back does not read priorities). This is the same blind spot as I7,
  now hit by a fault Claude did not choose.
- **R3.** HAVEN panics with "SMMUv3 initialization failed" before it builds
  any stage-2 table, as predicted for the boot check. Read-back did not
  report it: `pt_roots` is still zero, the walker raised `ValueError: PA
  0x0 outside the dumped image`, and Python exited with status 1, which the
  runner maps to "findings". The "findings" in `runs/held-out/results.md`
  for R3 is therefore wrong; the true outcome is a tool crash with no
  report. Tool defect RB1: read-back must report "HAVEN halted before
  stage-2 setup" (a probe result) instead of crashing, and the runner must
  not treat an exit status as a finding without a report.
- **R4.** A successful partition B mapping now unmaps partition A and
  panics. Read-back reports missing bindings on all five of A's regions,
  DMA enforcement drift on the virtio device (SID 8) and a missing binding
  for SPI 33, exactly the three kinds expected. The boot check also fails,
  so this fault is not caught only by read-back.

**Score.** 10 of 12 expectations held. Of the two misses, one is a wrong
prediction (R1: the unit tests are stronger than Claude assumed) and one
is a read-back robustness defect (R3, RB1). Read-back caught nothing the
boot check missed in this draw; the one fault that every method missed (R2)
falls in a blind spot already known from I7. Four random faults are too
few for a detection rate; they are a check that the method does not only
work on faults its author picked.

## Observations

- M5's extra finding is a correct consequence of the change: the read-only
  region's address is derived from partition B's size, so it moved too.
- P2: with its BAR unmapped, partition A's driver spins on a faulting read;
  HAVEN logs every stage-2 denial, 143,139 lines in 20 s. An unbounded log
  is a denial-of-service path for the console (observation; no ledger
  entry yet).

## Threats to validity

Claude wrote the cases, the tool and the runner; the owner approved the
verdicts but did not choose the faults. The four held-out faults were drawn
at random rather than chosen, but their expectations are still Claude's,
and four is a small sample. One
platform (QEMU virt, single security state), one device per bus.
