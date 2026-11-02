# G2 DMA checks (haven f4da7e8)

| Check | CPU | SMMU | Device | Partition A reports | UART as expected | Read-back findings as expected | Extra findings | SMMU events (SID 8, outside window / inside window) |
| - | - | - | - | - | - | - | - | - |
| G0 | cortex-a72 | nested | - | DMA device absent; DMA test done | yes | yes | - | 0 / 0 |
| G0 | max | nested | - | DMA device absent; DMA test done | yes | yes | - | 0 / 0 |
| G1 | cortex-a72 | nested | virtio-rng-pci,disable-legacy=on,iommu_platform=on | DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 0 |
| G1 | max | nested | virtio-rng-pci,disable-legacy=on,iommu_platform=on | DMA inside window OK, bytes written 0x40; DMA outside window blocked, buffer unchanged, completed 0x1; DMA test done | yes | yes | - | 17 / 0 |
| G2 | cortex-a72 | nested | virtio-rng-pci | DMA device bypasses the IOMMU (no VIRTIO_F_ACCESS_PLATFORM), not started; DMA test done | yes | yes | - | 0 / 0 |
| G2 | max | nested | virtio-rng-pci | DMA device bypasses the IOMMU (no VIRTIO_F_ACCESS_PLATFORM), not started; DMA test done | yes | yes | - | 0 / 0 |
| G3 | cortex-a72 | 1 | virtio-rng-pci,disable-legacy=on,iommu_platform=on | no DMA device granted; DMA test done | yes | yes | - | 0 / 0 |
| G3 | max | 1 | virtio-rng-pci,disable-legacy=on,iommu_platform=on | no DMA device granted; DMA test done | yes | yes | - | 0 / 0 |
| G4 | cortex-a72 | nested | virtio-rng-pci,disable-legacy=on,iommu_platform=on | DMA inside window OK, bytes written 0x40; DMA outside window NOT blocked, bytes written 0x40; DMA test done | yes | yes | - | 0 / 0 |
| G4 | max | nested | virtio-rng-pci,disable-legacy=on,iommu_platform=on | DMA inside window OK, bytes written 0x40; DMA outside window NOT blocked, bytes written 0x40; DMA test done | yes | yes | - | 0 / 0 |
