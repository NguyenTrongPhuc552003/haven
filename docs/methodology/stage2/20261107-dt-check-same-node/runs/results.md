# dt_check same-node test runs

| Tree | Change | Label | dt_check | Score | Boot | Denied accesses |
| - | - | - | - | - | - | - |
| S00 | none (T1) | False | matches | - | completes | 0 |
| S01 | memory node: base 0x40100000, size 0x40000000 | True | /memory@40000000: memory 0x40100000+0x40000000 not granted to partition 1 | TP | does not complete | 794624 |
| S02 | KDAL node: base 0x10003000, SPI 74 | True | /kdal-uart@9000000: SPI 106 not granted to partition 1 | TP | completes | 0 |
| S03 | KDAL node: base 0x10003000, SPI 1 edge | False | matches | TN | completes | 0 |
| S04 | KDAL node: base 0x10003000, SPI 2 | True | /kdal-uart@9000000: SPI 34 not granted to partition 1 | TP | completes | 0 |
| S05 | memory node: base 0x40100000, size 0x1f000000 | False | matches | TN | completes | 0 |
| S06 | timer node: secure PPI 9, virtual PPI 10 | True | /timer: PPI 26 not granted to partition 1 | TP | does not complete | 0 |
| S07 | KDAL node: size 0x2000, SPI 1 edge | True | /kdal-uart@9000000: registers 0x9000000+0x2000 not granted to partition 1 | TP | completes | 0 |
| S08 | KDAL node: base 0x09000800, size 0x800 | False | matches | TN | completes | 0 |
| S09 | KDAL node: base 0x10000000, size 0x2000 | False | matches | TN | completes | 0 |
| S10 | KDAL node: base 0x09010000, size 0x1001 | True | /kdal-uart@9000000: registers 0x9010000+0x1001 not granted to partition 1 | TP | completes | 0 |
| S11 | timer node: secure PPI 9, virtual PPI 12 | True | /timer: PPI 28 not granted to partition 1 | TP | does not complete | 0 |
| S12 | timer node: secure PPI 9, virtual PPI 15 | True | /timer: PPI 31 not granted to partition 1 | TP | does not complete | 0 |

Score over the drawn faults: {'TP': 8, 'FP': 0, 'TN': 4, 'FN': 0}
