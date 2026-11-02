# GIC read-back feasibility (haven 401f5da)

| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Read-back only |
| - | - | - | - | - | - | - | - |
| I0 | cortex-a72 | matches grant | yes | - | pass | pass | control |
| I0 | max | matches grant | yes | - | pass | pass | control |
| I1 | cortex-a72 | findings | yes | - | pass | pass | yes |
| I1 | max | findings | yes | - | pass | pass | yes |
| I2 | cortex-a72 | findings | yes | - | pass | pass | yes |
| I2 | max | findings | yes | - | pass | pass | yes |
| I3 | cortex-a72 | findings | yes | - | pass | pass | yes |
| I3 | max | findings | yes | - | pass | pass | yes |
| I4 | cortex-a72 | findings | yes | - | pass | pass | yes |
| I4 | max | findings | yes | - | pass | pass | yes |
| I5 | cortex-a72 | findings | yes | CPU 1 PPIs: interrupt enable drift; CPU 2 PPIs: interrupt enable drift; CPU 3 PPIs: interrupt enable drift | pass | pass | yes |
| I5 | max | findings | yes | CPU 1 PPIs: interrupt enable drift; CPU 2 PPIs: interrupt enable drift; CPU 3 PPIs: interrupt enable drift | pass | pass | yes |
| I6 | cortex-a72 | findings | yes | - | pass | pass | yes |
| I6 | max | findings | yes | - | pass | pass | yes |
| C1 | cortex-a72 | matches grant | yes | - | pass | pass | control |
| C1 | max | matches grant | yes | - | pass | pass | control |
