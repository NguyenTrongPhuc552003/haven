# Second blind test of dt_check.py: results

**Run:** 2026-11-07, macOS, haven `4227327` (dt_check, catalogue, pair
generator and runner match `freeze/SHA256SUMS`). Draw and predictions
committed in `4227327` before this run. Raw output: `runs/P00..P12/` and
`runs/results.md`.

## dt_check

| Pair | Faults | Label | dt_check findings | Score |
| - | - | - | - | - |
| P01 | secure timer PPI 9 + a second CPU | inside | none | TN |
| P02 | virtual timer PPI 15 + 3 CPUs | outside | CPU count; PPI 31 | TP |
| P03 | initramfs at 0x3F000000 + GPIO node | outside | initramfs not in granted RAM; GPIO registers | TP |
| P04 | 4 CPUs + initramfs +0x17000000 | outside | CPU count | TP |
| P05 | GPIO node disabled + memory 511 MiB | inside | none | TN |
| P06 | initramfs +16 MiB + memory 768 MiB | outside | memory | TP |
| P07 | 3 CPUs + KDAL node removed | outside | CPU count | TP |
| P08 | virtual timer PPI 15 + GPIO node disabled | outside | PPI 31 | TP |
| P09 | memory 0x40100000 + 511 MiB + 4 CPUs | outside | CPU count | TP |
| P10 | memory 768 MiB + virtual timer PPI 10 | outside | memory; PPI 26 | TP |
| P11 | secure timer PPI 9 + memory 512 MiB + 4 KiB | outside | memory | TP |
| P12 | memory 496 MiB + initramfs +0x17F00000 | outside | initramfs not in granted RAM | TP |

**10 true positives, 2 true negatives, no errors.** In every pair the
findings are exactly the union of the two faults' single findings: no
fault hid another and none added a finding. The control P00 matches.

## Boots

| Pair | Predicted | Observed | |
| - | - | - | - |
| P00, P01, P05, P07 | completes | completes | held |
| P02, P08 | does not complete | stops without timer interrupts | held |
| P03, P04 | kernel panic | kernel panic (no root), no denied access | held |
| P06, P10 | does not complete, denied accesses | does not complete, 589,824 denied accesses | held |
| P11 | does not complete, denied accesses | does not complete, 16 denied accesses, all on the one extra page at IPA 0x60000000 | held |
| P09 | does not complete (low confidence) | **completes**: Linux reports 256 pages unavailable below the memory node and boots | **missed** |
| P12 | kernel panic | kernel panic, but with 262,144 denied accesses | held, **reason wrong** |

## Against the predictions

dt_check: 13 of 13 held (12 pairs and the control). Boots: 12 of 13
held (P09 missed). One stated reason was wrong: P12 was predicted to
panic because Linux would drop an initramfs outside its memory node.
Linux did not drop it: it unpacked from 0x5FF00000 to 0x600314E8, past
the end of granted RAM; HAVEN denied the 262,144 reads above 0x60000000
(they read as zero), so the initramfs was corrupt and the kernel found no
root.

## Observations

- The union property held in all 12 pairs: for faults in different parts
  of the tree, dt_check's findings compose. Pairs inside the same node were
  excluded by design and remain untested.
- P12 is a case where the tree's fault reached the stage-2 boundary and
  HAVEN's enforcement held: the guest read past its grant, got zeros, and
  failed on its own. dt_check named the cause before boot.
- Across both blind tests (24 drawn trees), dt_check made no error
  against its specification. Containment is still a partial check: in
  M02 the only fault was inside the grant and Linux panicked; in P04
  dt_check reported the extra CPUs (which did no harm: CPU_ON is refused)
  and nothing about the initramfs address, which is what stopped Linux.
