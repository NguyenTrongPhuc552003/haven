# Draw and predictions

**Drawn** 2026-11-07 with seed text `haven 6fb4751 | owner 2026-11-07
02:31: 1. Approve 2. Approve 3. Approve, we have our own data/ directory
for your discovery and investigation about our SBC's specifications.`
(SHA-256 `eca52b23...67e32`, full value in `draw.json`). `drawn/` holds
the pairs applied to the frozen T1 tree. Predictions by Claude, committed
before dt_check or any boot ran on these trees.

10 pairs are labelled outside the grant, 2 inside. Prediction for
dt_check: every pair reports exactly the union of its two faults'
single findings (no fault hides or adds another).

| Pair | Faults | Label | dt_check (predicted) | Boot (predicted) | Confidence and why |
| - | - | - | - | - | - |
| P00 | none (T1) | inside | matches | completes | control |
| P01 | secure timer PPI 9 + a second CPU | inside | matches | completes | high: Linux at EL1 uses the virtual timer; CPU 1 refused as in M01 |
| P02 | virtual timer PPI 15 + 3 CPUs | outside | PPI 31; CPU count | does not complete | high: no timer interrupt (as M09) |
| P03 | initramfs at 0x3F000000 + GPIO node enabled | outside | initramfs range; GPIO registers | kernel panic | medium: the initramfs lies outside the memory node, so Linux drops it and finds no root |
| P04 | 4 CPUs + initramfs +0x17000000 | outside | CPU count | kernel panic | high: the initramfs is not at the address given (as M02) |
| P05 | GPIO node disabled + memory 511 MiB | inside | matches | completes | high |
| P06 | initramfs +16 MiB + memory 768 MiB | outside | memory | does not complete, denied accesses | medium: Linux allocates from the top of memory first and reaches ungranted RAM before it needs the initramfs (as T4) |
| P07 | 3 CPUs + KDAL node removed | outside | CPU count | completes | medium: KDAL loads with no device; its init still ends with `KDAL-DEMAND-END` |
| P08 | virtual timer PPI 15 + GPIO node disabled | outside | PPI 31 | does not complete | high |
| P09 | memory 0x40100000 + 511 MiB + 4 CPUs | outside | CPU count | does not complete | low: the first 1 MiB of the kernel image lies below the memory node |
| P10 | memory 768 MiB + virtual timer PPI 10 | outside | memory; PPI 26 | does not complete, denied accesses | high |
| P11 | secure timer PPI 9 + memory 512 MiB + 4 KiB | outside | memory | does not complete, denied accesses | medium: the one extra page is at the top, where Linux allocates first |
| P12 | memory 496 MiB + initramfs +0x17F00000 | outside | initramfs range | kernel panic | medium: the initramfs is outside the memory node and dropped |

Predicted score: 10 true positives, 2 true negatives.
