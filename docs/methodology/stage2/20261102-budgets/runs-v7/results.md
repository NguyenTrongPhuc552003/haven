# Budget checks (haven ccddc42)

| Check | CPU | Partition A duty | Partition B duty | Read-back findings | As expected |
| - | - | - | - | - | - |
| P2 | cortex-a72 | 957 permille, gap 1474 us, 11 gaps | 952 permille, gap 1983 us, 14 gaps | CPU 0 PPI 26: missing interrupt binding; CPU 2 PPI 26: missing interrupt binding | no |
| P2 | max | 1000 permille, gap 0 us, 0 gaps | 1000 permille, gap 0 us, 0 gaps | CPU 0 PPI 26: missing interrupt binding; CPU 2 PPI 26: missing interrupt binding | yes |
| P3 | cortex-a72 | 588 permille, gap 9923 us, 12 gaps | 978 permille, gap 2641 us, 4 gaps | CPU 2 PPI 26: missing interrupt binding | no |
| P3 | max | 591 permille, gap 10004 us, 13 gaps | 998 permille, gap 246 us, 1 gaps | CPU 2 PPI 26: missing interrupt binding | no |
| P4 | cortex-a72 | 966 permille, gap 789 us, 21 gaps | 946 permille, gap 1134 us, 25 gaps | CPU 0 PPI 26: interrupt priority drift; CPU 2 PPI 26: interrupt priority drift | no |
| P4 | max | 974 permille, gap 995 us, 10 gaps | 973 permille, gap 1828 us, 8 gaps | CPU 0 PPI 26: interrupt priority drift; CPU 2 PPI 26: interrupt priority drift | no |
