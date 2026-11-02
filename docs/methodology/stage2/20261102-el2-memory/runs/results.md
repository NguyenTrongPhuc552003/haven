# EL2 stage-1 checks (haven d0d5002)

| Check | CPU | UART lines | Forbidden seen | B stage (exp) | Read-back findings | As expected |
| - | - | - | - | - | - | - |
| N1 | cortex-a72 | 3 of 3 | - | 5 (5) | none | yes |
| N1 | max | 3 of 3 | - | 5 (5) | none | yes |
| N2 | cortex-a72 | 3 of 3 | - | 5 (5) | EL2 / partition 2: partition memory mapped at EL2 | yes |
| N2 | max | 3 of 3 | - | 5 (5) | EL2 / partition 2: partition memory mapped at EL2 | yes |
| N3 | cortex-a72 | 3 of 3 | - | 0 (0) | none | yes |
| N3 | max | 3 of 3 | - | 0 (0) | none | yes |
| N4 | cortex-a72 | 1 of 1 | - | 0 (0) | not run | yes |
| N4 | max | 1 of 1 | - | 0 (0) | not run | yes |
| N5 | cortex-a72 | 3 of 3 | - | 5 (5) | none | yes |
| N5 | max | 3 of 3 | - | 5 (5) | none | yes |
