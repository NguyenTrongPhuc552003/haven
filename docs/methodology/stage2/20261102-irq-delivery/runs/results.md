# Interrupt delivery checks (haven 3fbafae)

| Check | CPU | Device | Partition A reports | UART as expected | Read-back findings as expected | Extra findings | SMMU events (total / consumed by HAVEN) | Consumption as expected |
| - | - | - | - | - | - | - | - | - |
| J1 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; waiting for the UART interrupt; UART interrupt delivered (INTID 33) | yes | yes | - | 17 / 17 | yes |
| J1 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; waiting for the UART interrupt; UART interrupt delivered (INTID 33) | yes | yes | - | 17 / 17 | yes |
| J2 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; UART interrupt NOT delivered | yes | yes | - | 17 / 17 | yes |
| J2 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; UART interrupt NOT delivered | yes | yes | - | 17 / 17 | yes |
| J3 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; UART interrupt NOT delivered | yes | yes | - | 17 / 17 | yes |
| J3 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; UART interrupt NOT delivered | yes | yes | - | 17 / 17 | yes |
| J4 | cortex-a72 | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; waiting for the UART interrupt; UART interrupt delivered (INTID 33) | yes | yes | - | 17 / 17 | yes |
| J4 | max | yes | config page read-only; DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done; waiting for the UART interrupt; UART interrupt delivered (INTID 33) | yes | yes | - | 17 / 17 | yes |
