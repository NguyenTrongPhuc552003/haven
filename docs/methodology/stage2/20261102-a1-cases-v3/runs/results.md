# A1 case set v3 results (haven 4174daf)

| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Caught only by read-back |
| - | - | - | - | - | - | - | - |
| M3 | cortex-a72 | findings | yes | - | fail | fail | no |
| M3 | max | findings | yes | - | fail | fail | no |
| M4 | cortex-a72 | findings | yes | - | fail | pass | no |
| M4 | max | findings | yes | - | fail | pass | no |
| M5 | cortex-a72 | findings | yes | A RO: association drift | pass | pass | yes |
| M5 | max | findings | yes | A RO: association drift | pass | pass | yes |
| S8 | cortex-a72 | findings | yes | - | pass | pass | yes |
| S8 | max | findings | yes | - | pass | pass | yes |
| S9 | cortex-a72 | findings | no (unexpected finding) | pci-1af4:1044: DMA enforcement drift | fail | pass | blind spot |
| S9 | max | findings | no (unexpected finding) | pci-1af4:1044: DMA enforcement drift | fail | pass | blind spot |
| I7 | cortex-a72 | matches grant | yes | - | pass | pass | blind spot |
| I7 | max | matches grant | yes | - | pass | pass | blind spot |
| I8 | cortex-a72 | findings | yes | - | pass | pass | yes |
| I8 | max | findings | yes | - | pass | pass | yes |
| P1 | cortex-a72 | matches grant | yes | - | pass | pass | blind spot |
| P1 | max | matches grant | yes | - | pass | pass | blind spot |
| P2 | cortex-a72 | findings | yes | - | pass | pass | yes |
| P2 | max | findings | yes | - | pass | pass | yes |
| C6 | cortex-a72 | matches grant | yes | - | pass | pass | control |
| C6 | max | matches grant | yes | - | pass | pass | control |
| C7 | cortex-a72 | matches grant | yes | - | pass | pass | control |
| C7 | max | matches grant | yes | - | pass | pass | control |
