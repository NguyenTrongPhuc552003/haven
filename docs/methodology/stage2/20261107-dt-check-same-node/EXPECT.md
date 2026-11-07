# Draw, labels and predictions

**Drawn** 2026-11-07 with seed text `haven cf6405b | owner 2026-11-07
21:17: 2. Approve` (SHA-256 `1023dca1...8f594`, full value in
`draw.json`). Labels (`labels.json`) and predictions by Claude, written
after the draw and committed before dt_check or any boot ran.

Granted to partition 1 (`grant-l4.json`): Normal RAM 0x40000000 to
0x60000000; device regions UART 0x09000000+0x1000 and PCI BAR
0x10000000+0x4000 (and PCI config, unused here); SPI INTID 33 only; PPI
27 on CPU 0; CPUs 0 and 1.

| Tree | Change | Parts alone (single-fault labels) | Merged node | Label | dt_check (predicted) | Boot (predicted) |
| - | - | - | - | - | - | - |
| S00 | none (T1, control) | | | inside | matches | completes |
| S01 | memory base 0x40100000, size 1 GiB | 1 GiB: outside | ends 0x80100000 > 0x60000000 | outside | memory | does not complete, denied accesses (as T4) |
| S02 | KDAL base 0x10003000, SPI 74 | base: inside; SPI 74: outside | registers end 0x10004000, inside the BAR; INTID 106 is partition 0's | outside | INTID 106 | completes (KDAL only maps its registers) |
| S03 | KDAL base 0x10003000, SPI 1 edge | both inside | inside the BAR; INTID 33 | inside | matches | completes |
| S04 | KDAL base 0x10003000, SPI 2 | base: inside; SPI 2: outside | INTID 34 not granted | outside | INTID 34 | completes |
| S05 | memory base 0x40100000, size 496 MiB | 496 MiB: inside | 0x40100000 to 0x5F100000, inside | inside | matches | completes (kernel below the memory node booted in P09) |
| S06 | timer secure PPI 9, virtual PPI 10 | secure 9: inside; virtual 10: outside | virtual timer INTID 26 | outside | PPI 26 | does not complete |
| S07 | KDAL size 0x2000, SPI 1 edge | size: outside; edge: inside | 0x09000000+0x2000 crosses the UART page | outside | registers | completes (as T2) |
| S08 | KDAL base 0x09000800, size 0x800 | base: **outside**; size: inside | 0x09000800 to 0x09001000, inside the UART page | **inside** | matches | completes |
| S09 | KDAL base 0x10000000, size 0x2000 | base: inside; size: **outside** | 0x10000000 to 0x10002000, inside the 16 KiB BAR | **inside** | matches | completes |
| S10 | KDAL base 0x09010000, size 0x1001 | both outside | RTC region, not granted | outside | registers | completes |
| S11 | timer secure PPI 9, virtual PPI 12 | virtual 12: outside | INTID 28 | outside | PPI 28 | does not complete |
| S12 | timer secure PPI 9, virtual PPI 15 | virtual 15: outside | INTID 31 | outside | PPI 31 | does not complete |

8 outside, 4 inside. **S08 and S09 are the interaction cases**: one part
alone is outside the grant, the merged node is inside. A checker that
judged the parts, or that the single-fault results would mislead, would
report them. Predicted score: 8 true positives, 4 true negatives.
