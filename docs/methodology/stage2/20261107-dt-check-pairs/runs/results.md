# dt_check pair test runs

| Pair | Faults | Label | dt_check | Score | Boot | Denied accesses |
| - | - | - | - | - | - | - |
| P00 | none (T1) | False | matches | - | completes | 0 |
| P01 | O7 secure timer PPI 9 + O8 1 more (2 CPUs) | False | matches | TN | completes | 0 |
| P02 | O7 virtual timer PPI 15 + O8 2 more (3 CPUs) | True | /cpus: 3 CPU nodes, partition 1 has 2 CPU(s); /timer: PPI 31 not granted to partition 1 | TP | does not complete | 0 |
| P03 | O3 -0x09000000 + O9 GPIO 0x09030000, okay | True | /chosen: initramfs 0x3f000000-0x3f1314e8 not in RAM granted to partition 1; /extra@9030000: registers 0x9030000+0x1000 not granted to partition 1 | TP | kernel panic | 0 |
| P04 | O8 3 more (4 CPUs) + O3 +0x17000000 | True | /cpus: 4 CPU nodes, partition 1 has 2 CPU(s) | TP | kernel panic | 0 |
| P05 | O9 GPIO 0x09030000, disabled + O2 0x40000000 + 511 MiB | False | matches | TN | completes | 0 |
| P06 | O3 +0x01000000 + O1 768 MiB | True | /memory@40000000: memory 0x40000000+0x30000000 not granted to partition 1 | TP | does not complete | 589824 |
| P07 | O8 2 more (3 CPUs) + O10 KDAL node removed | True | /cpus: 3 CPU nodes, partition 1 has 2 CPU(s) | TP | completes | 0 |
| P08 | O7 virtual timer PPI 15 + O9 GPIO 0x09030000, disabled | True | /timer: PPI 31 not granted to partition 1 | TP | does not complete | 0 |
| P09 | O2 0x40100000 + 511 MiB + O8 3 more (4 CPUs) | True | /cpus: 4 CPU nodes, partition 1 has 2 CPU(s) | TP | completes | 0 |
| P10 | O1 768 MiB + O7 virtual timer PPI 10 | True | /memory@40000000: memory 0x40000000+0x30000000 not granted to partition 1; /timer: PPI 26 not granted to partition 1 | TP | does not complete | 589824 |
| P11 | O7 secure timer PPI 9 + O1 512 MiB + 4 KiB | True | /memory@40000000: memory 0x40000000+0x20001000 not granted to partition 1 | TP | does not complete | 16 |
| P12 | O1 496 MiB + O3 +0x17F00000 | True | /chosen: initramfs 0x5ff00000-0x600314e8 not in RAM granted to partition 1 | TP | kernel panic | 262144 |

Score over the drawn faults: {'TP': 10, 'FP': 0, 'TN': 2, 'FN': 0}
