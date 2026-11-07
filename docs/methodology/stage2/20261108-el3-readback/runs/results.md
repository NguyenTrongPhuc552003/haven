# EL3 against monitor read-back

| Case | Monitor read-back | EL3 read-back | Same findings |
| - | - | - | - |
| E0 | CPU 2 PPI 27: missing interrupt binding (marker) | CPU 2 PPI 27: missing interrupt binding | yes |
| M3 | A RAM: execute drift; B RAM: execute drift; CPU 2 PPI 27: missing interrupt binding (time limit (40 s, partition A ) | A RAM: execute drift; B RAM: execute drift; CPU 2 PPI 27: missing interrupt binding | yes |
| M4 | A PCI BAR: memory-type drift; A PCI BAR: shareability drift; A PCI config: memory-type drift; A PCI config: shareability drift; A UART: memory-type drift; A UART: shareability drift; CPU 2 PPI 27: missing interrupt binding; EL2 / partition 1: partition memory mapped at EL2 (marker) | A PCI BAR: memory-type drift; A PCI BAR: shareability drift; A PCI config: memory-type drift; A PCI config: shareability drift; A UART: memory-type drift; A UART: shareability drift; CPU 2 PPI 27: missing interrupt binding; EL2 / partition 1: partition memory mapped at EL2 | yes |
| M5 | A RO: association drift; B RAM: extent drift; CPU 2 PPI 27: missing interrupt binding (marker) | A RO: association drift; B RAM: extent drift; CPU 2 PPI 27: missing interrupt binding | yes |
| S8 | CPU 2 PPI 27: missing interrupt binding; SID 8: DMA access drift (marker) | CPU 2 PPI 27: missing interrupt binding; SID 8: DMA access drift | yes |
| I1 | CPU 2 PPI 27: missing interrupt binding; SPI 33: missing interrupt binding (marker) | CPU 2 PPI 27: missing interrupt binding; SPI 33: missing interrupt binding | yes |
| I4 | CPU 2 PPI 27: missing interrupt binding (marker) | CPU 2 PPI 27: missing interrupt binding | yes |
| I5 | CPU 0 PPIs: interrupt enable drift; CPU 2 PPI 27: missing interrupt binding (marker) | CPU 0 PPIs: interrupt enable drift; CPU 2 PPI 27: missing interrupt binding | yes |
| S1 | CPU 0: CPU assignment drift; CPU 2 PPI 27: missing interrupt binding; SPI 33: missing interrupt binding (marker) | CPU 0: CPU assignment drift; CPU 2 PPI 27: missing interrupt binding; SPI 33: missing interrupt binding | yes |
