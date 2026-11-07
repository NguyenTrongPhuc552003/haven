# Third blind test of dt_check.py: results

**Run:** 2026-11-07, macOS, haven `9917ab2` (dt_check, catalogue,
generator and runner match `freeze/SHA256SUMS`). Draw, labels and
predictions committed in `9917ab2` before this run. Raw output:
`runs/S00..S12/` and `runs/results.md`.

| Tree | Change | Label | dt_check | Score | Boot |
| - | - | - | - | - | - |
| S00 | none (control) | inside | matches | - | completes |
| S01 | memory base 0x40100000, 1 GiB | outside | memory 0x40100000+0x40000000 | TP | does not complete, 794,624 denied accesses |
| S02 | KDAL base 0x10003000, SPI 74 | outside | INTID 106 | TP | completes |
| S03 | KDAL base 0x10003000, SPI 1 edge | inside | matches | TN | completes |
| S04 | KDAL base 0x10003000, SPI 2 | outside | INTID 34 | TP | completes |
| S05 | memory base 0x40100000, 496 MiB | inside | matches | TN | completes |
| S06 | timer secure 9, virtual 10 | outside | PPI 26 | TP | does not complete |
| S07 | KDAL size 0x2000, SPI 1 edge | outside | registers 0x9000000+0x2000 | TP | completes |
| S08 | KDAL base 0x09000800, size 0x800 | inside | matches | TN | completes |
| S09 | KDAL base 0x10000000, size 0x2000 | inside | matches | TN | completes |
| S10 | KDAL base 0x09010000, size 0x1001 | outside | registers 0x9010000+0x1001 | TP | completes |
| S11 | timer secure 9, virtual 12 | outside | PPI 28 | TP | does not complete |
| S12 | timer secure 9, virtual 15 | outside | PPI 31 | TP | does not complete |

**8 true positives, 4 true negatives, no errors.** All 13 dt_check
predictions and all 13 boot predictions held.

**Observations.**
- The two interaction cases behaved as the merged node requires: S08
  (a register base that alone crossed the UART page, with a size that
  keeps it inside) and S09 (a size that alone left the UART page, moved
  into the 16 KiB PCI BAR) are inside the grant, and dt_check reported
  nothing. dt_check judges the node as written, not its parts.
- In every outside case, dt_check named the field that left the grant
  (memory range, register range, INTID or virtual timer PPI), and only
  that field.

**Across the three blind tests** (36 drawn trees, all from the same
catalogue and grant): 23 true positives, 13 true negatives, no false
positive or negative. Limits unchanged: the labels and dt_check share an
author and a specification, and the catalogue covers memory, initramfs,
device register, interrupt, timer and CPU faults of one grant on QEMU.
