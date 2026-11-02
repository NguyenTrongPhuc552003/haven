# A1 case set v3, held-out cases (haven ce220bb)

| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Caught only by read-back |
| - | - | - | - | - | - | - | - |
| R1 | cortex-a72 | matches grant | see expect.txt | - | fail | pass | - |
| R1 | max | matches grant | see expect.txt | - | fail | pass | - |
| R2 | cortex-a72 | matches grant | see expect.txt | - | pass | pass | - |
| R2 | max | matches grant | see expect.txt | - | pass | pass | - |
| R3 | cortex-a72 | findings | see expect.txt | - | pass | fail | - |
| R3 | max | findings | see expect.txt | - | pass | fail | - |
| R4 | cortex-a72 | findings | see expect.txt | A PCI BAR: missing binding; A PCI config: missing binding; A RAM: missing binding; A RO: missing binding; A UART: missing binding; SPI 33: missing interrupt binding; pci-1af4:1044: DMA enforcement drift | pass | fail | - |
| R4 | max | findings | see expect.txt | A PCI BAR: missing binding; A PCI config: missing binding; A RAM: missing binding; A RO: missing binding; A UART: missing binding; SPI 33: missing interrupt binding; pci-1af4:1044: DMA enforcement drift | pass | fail | - |
