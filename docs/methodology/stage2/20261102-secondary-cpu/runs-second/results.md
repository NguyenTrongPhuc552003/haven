# Secondary-CPU checks (haven e364526)

| Check | CPU | Expected lines present | Forbidden lines seen | B stage (expected) | As expected |
| - | - | - | - | - | - |
| L1 | cortex-a72 | 2 of 2 | - | 5 (5) | yes |
| L1 | max | 2 of 2 | - | 5 (5) | yes |
| L2 | cortex-a72 | 2 of 2 | - | 0 (0) | yes |
| L2 | max | 2 of 2 | - | 0 (0) | yes |
| L3 | cortex-a72 | 2 of 2 | - | 0 (0) | yes |
| L3 | max | 2 of 2 | - | 0 (0) | yes |
| L4 | cortex-a72 | 1 of 1 | - | 0 (0) | yes |
| L4 | max | 1 of 1 | - | 0 (0) | yes |
| L5 | cortex-a72 | 1 of 2 | CPU 2 not ready within 100 ms - partition B not started | 0 (0) | no |
| L5 | max | 1 of 2 | CPU 2 not ready within 100 ms - partition B not started | 0 (0) | no |
