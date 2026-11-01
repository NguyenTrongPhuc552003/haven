# A1 case-set results (haven b1dabd8)

| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Counts for A1 |
| - | - | - | - | - | - | - | - |
| D1 | cortex-a72 | probe failure | no verdict | - | pass | fail | no |
| D1 | max | probe failure | no verdict | - | pass | fail | no |
| D2 | cortex-a72 | drift | yes | *: cross-partition alias | pass | fail | no |
| D2 | max | drift | yes | *: cross-partition alias | pass | fail | no |
| D3 | cortex-a72 | drift | yes | - | fail | pass | no |
| D3 | max | drift | yes | - | fail | pass | no |
| D4 | cortex-a72 | drift | yes | B RAM: missing binding | pass | pass | yes |
| D4 | max | drift | yes | B RAM: missing binding | pass | pass | yes |
| D5 | cortex-a72 | drift | yes | - | fail | pass | no |
| D5 | max | drift | yes | - | fail | pass | no |
| D6 | cortex-a72 | probe failure | no verdict | - | pass | fail | no |
| D6 | max | probe failure | no verdict | - | pass | fail | no |
| D7 | cortex-a72 | drift | yes | - | fail | pass | no |
| D7 | max | drift | yes | - | fail | pass | no |
| C1 | cortex-a72 | H matches G | yes | - | pass | pass | control |
| C1 | max | H matches G | yes | - | pass | pass | control |
| C2 | cortex-a72 | H matches G | yes | - | fail | pass | control |
| C2 | max | H matches G | yes | - | fail | pass | control |
| C3 | cortex-a72 | H matches G | yes | - | pass | pass | control |
| C3 | max | H matches G | yes | - | pass | pass | control |
