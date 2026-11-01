# A1 case set v2 results (haven 3517927)

| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Read-back only |
| - | - | - | - | - | - | - | - |
| R1 | cortex-a72 | findings | yes | - | fail | pass | no |
| R1 | max | findings | yes | - | fail | pass | no |
| S1 | cortex-a72 | findings | yes | - | pass | pass | yes |
| S1 | max | findings | yes | - | pass | pass | yes |
| S2 | cortex-a72 | findings | yes | - | pass | pass | yes |
| S2 | max | findings | yes | - | pass | pass | yes |
| S5 | cortex-a72 | findings | yes | - | pass | fail | no |
| S5 | max | findings | yes | - | pass | fail | no |
| S6 | cortex-a72 | findings | yes | - | fail | fail | no |
| S6 | max | findings | yes | - | fail | fail | no |
| S7 | cortex-a72 | findings | yes | SID 8: DMA extent drift | fail | fail | no |
| S7 | max | findings | yes | SID 8: DMA extent drift | fail | fail | no |
| U1 | cortex-a72 | findings | yes | - | pass | pass | yes |
| U1 | max | findings | yes | - | pass | pass | yes |
| C1 | cortex-a72 | matches grant | yes | - | pass | pass | control |
| C1 | max | matches grant | yes | - | pass | pass | control |
| C5 | cortex-a72 | matches grant | yes | - | pass | pass | control |
| C5 | max | matches grant | yes | - | pass | pass | control |
