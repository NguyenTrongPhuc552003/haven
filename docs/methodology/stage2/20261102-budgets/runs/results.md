# Budget checks (haven 5d7d908)

| Check | CPU | Partition A duty | Partition B duty | Read-back findings | As expected |
| - | - | - | - | - | - |
| P1 | cortex-a72 | 527 permille, gap 9747 us, 17 gaps | 319 permille, gap 7780 us, 20 gaps | none | yes |
| P1 | max | 564 permille, gap 9521 us, 11 gaps | 319 permille, gap 7926 us, 20 gaps | none | yes |
| P2 | cortex-a72 | 997 permille, gap 356 us, 2 gaps | 1000 permille, gap 0 us, 0 gaps | CPU 0 PPI 26: missing interrupt binding; CPU 2 PPI 26: missing interrupt binding; CPU 0 PPI 26: interrupt priority drift; CPU 2 PPI 26: interrupt priority drift | no |
| P2 | max | 994 permille, gap 443 us, 4 gaps | 987 permille, gap 1683 us, 3 gaps | CPU 0 PPI 26: missing interrupt binding; CPU 2 PPI 26: missing interrupt binding; CPU 0 PPI 26: interrupt priority drift; CPU 2 PPI 26: interrupt priority drift | no |
| P3 | cortex-a72 | 627 permille, gap 9923 us, 11 gaps | 1000 permille, gap 0 us, 0 gaps | CPU 2 PPI 26: missing interrupt binding; CPU 2 PPI 26: interrupt priority drift | no |
| P3 | max | 619 permille, gap 9524 us, 11 gaps | 1000 permille, gap 0 us, 0 gaps | CPU 2 PPI 26: missing interrupt binding; CPU 2 PPI 26: interrupt priority drift | no |
| P4 | cortex-a72 | 992 permille, gap 1270 us, 2 gaps | 995 permille, gap 913 us, 1 gaps | CPU 0 PPI 26: interrupt priority drift; CPU 2 PPI 26: interrupt priority drift | no |
| P4 | max | 990 permille, gap 1523 us, 2 gaps | 1000 permille, gap 0 us, 0 gaps | CPU 0 PPI 26: interrupt priority drift; CPU 2 PPI 26: interrupt priority drift | no |
