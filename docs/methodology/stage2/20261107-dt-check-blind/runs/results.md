# dt_check blind test runs

| Fault | Operator | Parameter | Label | dt_check | Score | Boot | Denied accesses |
| - | - | - | - | - | - | - | - |
| M00 | none (T1) | - | False | matches | - | completes | 0 |
| M01 | extra CPU nodes | 1 more (2 CPUs) | False | matches | TN | completes | 0 |
| M02 | initramfs range shifted | +0x01000000 | False | matches | TN | kernel panic | 0 |
| M03 | memory node size | 496 MiB | False | matches | TN | completes | 0 |
| M04 | neutral edit | KDAL node renamed kdal@9000000 | False | matches | TN | completes | 0 |
| M05 | KDAL node register base | 0x09000800 | True | /kdal-uart@9000000: registers 0x9000800+0x1000 not granted to partition 1 | TP | completes | 0 |
| M06 | KDAL node interrupt | SPI 1 edge | False | matches | TN | completes | 0 |
| M07 | KDAL node register size | 0x2000 | True | /kdal-uart@9000000: registers 0x9000000+0x2000 not granted to partition 1 | TP | completes | 0 |
| M08 | timer node entries | virtual timer PPI 10 | True | /timer: PPI 26 not granted to partition 1 | TP | does not complete | 0 |
| M09 | timer node entries | virtual timer PPI 12 | True | /timer: PPI 28 not granted to partition 1 | TP | does not complete | 0 |
| M10 | extra CPU nodes | 3 more (4 CPUs) | True | /cpus: 4 CPU nodes, partition 1 has 2 CPU(s) | TP | completes | 0 |
| M11 | KDAL node register size | 0x800 | False | matches | TN | completes | 0 |
| M12 | neutral edit | KDAL node renamed kdal@9000000 | False | matches | TN | completes | 0 |

Score over the drawn faults: {'TP': 5, 'FP': 0, 'TN': 7, 'FN': 0}
