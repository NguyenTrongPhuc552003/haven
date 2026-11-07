# Read-back v12a reruns

| Case | Monitor read-back | EL3 read-back | Same findings |
| - | - | - | - |
| E0 | CPU 2 PPI 27: missing interrupt binding (marker) | CPU 2 PPI 27: missing interrupt binding | yes |
| I1 | CPU 2 PPI 27: missing interrupt binding; SPI 33: interrupt routing drift; SPI 33: missing interrupt binding (marker) | CPU 2 PPI 27: missing interrupt binding; SPI 33: interrupt routing drift; SPI 33: missing interrupt binding | yes |
| S1 | CPU 0: CPU assignment drift; CPU 2 PPI 27: missing interrupt binding; SPI 33: missing interrupt binding (marker) | CPU 0: CPU assignment drift; CPU 2 PPI 27: missing interrupt binding; SPI 33: missing interrupt binding | yes |
