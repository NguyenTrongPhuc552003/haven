# DMA hardening checks (haven d0ed794)

| Check | CPU | Device | Partition A reports | UART as expected | Read-back findings as expected | Extra findings | SMMU events (total / consumed by HAVEN) | Consumption as expected |
| - | - | - | - | - | - | - | - | - |
| H1 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 17 | yes |
| H1 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 17 | yes |
| H2 | cortex-a72 | - | no DMA device granted; DMA test done | yes | yes | - | 0 / 0 | yes |
| H2 | max | - | no DMA device granted; DMA test done | yes | yes | - | 0 / 0 | yes |
| H3 | cortex-a72 | yes | config write landed; DMA test done | yes | yes | - | 0 / 0 | yes |
| H3 | max | yes | config write landed; DMA test done | yes | yes | - | 0 / 0 | yes |
| H4 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 17 | yes |
| H4 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 17 | yes |
| H5 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 0 | yes |
| H5 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 0 | yes |
| H6 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 0 | yes |
| H6 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 0 | yes |
