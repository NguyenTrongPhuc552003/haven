# Campaign step C4: every case, one commit, one freeze (design)

**Status:** design and expectations, 2026-11-22, before the run.
Owner-approved as a step (2026-11-22, "Approve" to C4 then C5).

## What C4 is [P]

Every case of the campaign so far ran once, each set at its own commit
(written and corrected at `0c6d755`, random at `a32b66e`, historical at
`e9e2014`, scenario 2 at `9e19bb2`). C4 runs them all again at **one**
commit under **one** freeze, so the C5 results rest on a single tree,
and so a second run of every case shows whether any verdict is unstable.

Two runs of `campaign.yml` (it takes one cases file and one scenario):

| Run | Cases file | Cases |
| - | - | - |
| C4-boot | `tools/campaign/cases/c4-boot.json` | the 40 written cases with W03, W07, W12 and W23 replaced by their corrected forms, the 60 random and the 6 historical: 106 |
| C4-linux2 | `tools/campaign/cases/s2-linux2.json` | scenario 2: 3 controls and 40 random cases |

`c4/make_c4.py` writes `c4-boot.json` from the frozen C2, random and
historical cases files (their patches and classes unchanged; the
expectations of the written and corrected cases carried over).
`c4/freeze/SHA256SUMS-C4` covers the tools, the workflow, both cases
files and every patch, checked at the run's commit before the run.

## Expectations

| Row | Check | Expected |
| - | - | - |
| C4-1 | each case's verdict per method against its recorded run (runs 44920967125, 44920974193, 44925656868, 44929925011, 44937412628) | identical for every case and method |
| C4-2 | disagreements with the written expectations (`score.py`) | the recorded ones only: W08 M6 (`error`, HAVEN halts before stage-2 setup) and the controls' none; W09's M6 now agrees (read-back v15) |
| C4-3 | M6 run time | median within 0.3 s of the recorded runs' (0.9 to 1.2 s in scenario 1) |

If C4-1 fails for a case, that case runs five more times (the W09
repeat's method) before any cause is named: a verdict that changes
between runs of the same tree is a flaky method, recorded as its own
finding (E5), and the case is reported with both verdicts.

## Results (2026-11-22)

Both runs at `49fe767` (the merge of #189); `SHA256SUMS-C4` holds there
(138 files, all OK) and `make_c4.py --check` passes [F].
C4-boot: run 44989936131, ten shards; C4-linux2: run 44989942726, eight.

| Row | Result |
| - | - |
| C4-1 | **holds**: 149 cases, 1,043 method verdicts compared with their recorded runs, 1,043 identical, 0 different. No method is flaky on these cases |
| C4-2 | **holds**: one disagreement, W08's M6 (`error`: HAVEN halts before stage-2 setup), as recorded; C4-boot's summary job fails on it, by design |
| C4-3 | **holds**: M6's median 1.0 s in scenario 1 (105 runs, max 40.9 s, the halting cases); 4.7 s in scenario 2 (43 runs, cortex-a72 only) |

The per-class tables and what they mean are in `../RESULTS.md` (step C5).
