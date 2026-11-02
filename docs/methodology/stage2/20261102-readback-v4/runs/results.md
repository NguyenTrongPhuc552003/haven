# Read-back v4 regression checks (tool at haven a9bf185)

| Check | Code | CPU | Exit (expected) | Findings | As expected |
| - | - | - | - | - | - |
| V0 | ce220bb | cortex-a72 | 0 (0) | none | yes |
| V0 | ce220bb | max | 0 (0) | none | yes |
| R1 | ce220bb + R1 | cortex-a72 | 0 (0) | none | yes |
| R1 | ce220bb + R1 | max | 0 (0) | none | yes |
| R2 | ce220bb + R2 | cortex-a72 | 1 (1) | SPI 33: interrupt priority drift | yes |
| R2 | ce220bb + R2 | max | 1 (1) | SPI 33: interrupt priority drift | yes |
| R3 | ce220bb + R3 | cortex-a72 | 2 (2) | probe failure | yes |
| R3 | ce220bb + R3 | max | 2 (2) | probe failure | yes |
| R4 | ce220bb + R4 | cortex-a72 | 1 (1) | A RAM: missing binding; A UART: missing binding; A RO: missing binding; A PCI config: missing binding; A PCI BAR: missing binding; pci-1af4:1044: DMA enforcement drift; SPI 33: missing interrupt binding | yes |
| R4 | ce220bb + R4 | max | 1 (1) | A RAM: missing binding; A UART: missing binding; A RO: missing binding; A PCI config: missing binding; A PCI BAR: missing binding; pci-1af4:1044: DMA enforcement drift; SPI 33: missing interrupt binding | yes |
| I7 | ce220bb + edit | cortex-a72 | 1 (1) | SPI 33: interrupt priority drift | yes |
| I7 | ce220bb + edit | max | 1 (1) | SPI 33: interrupt priority drift | yes |
| W0 | HEAD | cortex-a72 | 0 (0) | none | yes |
| W0 | HEAD | max | 0 (0) | none | yes |
| W3 | HEAD + edit | cortex-a72 | 1 (1) | CPU 2 PPI 27: missing interrupt binding | yes |
| W3 | HEAD + edit | max | 1 (1) | CPU 2 PPI 27: missing interrupt binding | yes |
