# SMMU fix checks (haven d5bb678; host unit tests: pass)

| Check | CPU | SMMU stage | HAVEN grant call | Read-back | Expected findings | Extra findings | Boot check |
| - | - | - | - | - | - | - | - |
| B0 | cortex-a72 | nested | - | matches grant | yes | - | pass |
| B0 | max | nested | - | matches grant | yes | - | pass |
| B1 | cortex-a72 | 1 | - | matches grant | yes | - | pass |
| B1 | max | 1 | - | matches grant | yes | - | pass |
| F1 | cortex-a72 | nested | HAVEN: DMA grant SID 8 rc=0 | matches grant | yes | - | pass |
| F1 | max | nested | HAVEN: DMA grant SID 8 rc=0 | matches grant | yes | - | pass |
| F2 | cortex-a72 | nested | HAVEN: DMA grant SID 8 rc=0 | matches grant | yes | - | pass |
| F2 | max | nested | HAVEN: DMA grant SID 8 rc=0 | matches grant | yes | - | pass |
| F3 | cortex-a72 | 1 | HAVEN: DMA grant SID 8 rc=-95 | findings | yes | - | pass |
| F3 | max | 1 | HAVEN: DMA grant SID 8 rc=-95 | findings | yes | - | pass |
| M1 | cortex-a72 | nested | HAVEN: DMA grant SID 8 rc=0 | findings | yes | - | pass |
| M1 | max | nested | HAVEN: DMA grant SID 8 rc=0 | findings | yes | - | pass |
| M2 | cortex-a72 | nested | HAVEN: DMA grant SID 0 rc=0 | findings | yes | - | pass |
| M2 | max | nested | HAVEN: DMA grant SID 0 rc=0 | findings | yes | - | pass |
| M3 | cortex-a72 | nested | HAVEN: DMA grant SID 8 rc=0 | findings | yes | - | pass |
| M3 | max | nested | HAVEN: DMA grant SID 8 rc=0 | findings | yes | - | pass |
| M4 | cortex-a72 | nested | HAVEN: DMA grant SID 8 rc=0 | findings | yes | - | pass |
| M4 | max | nested | HAVEN: DMA grant SID 8 rc=0 | findings | yes | - | pass |
