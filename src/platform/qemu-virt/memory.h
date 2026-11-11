#ifndef HAVEN_PLATFORM_QEMU_VIRT_MEMORY_H
#define HAVEN_PLATFORM_QEMU_VIRT_MEMORY_H

/* QEMU virt machine (aarch64) memory map */
#define QEMU_UART_BASE 0x09000000UL

/* HAVEN's own console (CMake HAVEN_CONSOLE_UART): UART0, which partition A
 * also owns, or QEMU virt's second Normal-world PL011, which no partition
 * maps (docs/methodology/stage2/20261109-qemu-console). */
#if defined(HAVEN_CONSOLE_UART) && HAVEN_CONSOLE_UART == 1
#define QEMU_CONSOLE_BASE 0x09040000UL
#else
#define QEMU_CONSOLE_BASE QEMU_UART_BASE
#endif
#define QEMU_GIC_DIST_BASE 0x08000000UL
#define QEMU_GIC_REDIST_BASE 0x080A0000UL
#define QEMU_SMMU_BASE 0x09050000UL /* SMMUv3 for QEMU virt */
#define QEMU_TIMER_FREQ 62500000UL /* 62.5 MHz */
#define QEMU_NR_CPUS 4

/* DRAM layout: Haven loads at 0x40000000 */
#define QEMU_DRAM_BASE 0x40000000UL
#define QEMU_DRAM_SIZE 0x80000000UL /* 2 GiB */

/* Haven hypervisor default load address (matches linker HV_LOAD_ADDR).
 * Haven BSS holds the stage-2 page table pool (~5 MB); reserve 8 MB total
 * so partition A's PA base starts cleanly above the hypervisor image. */
#define HAVEN_LOAD_PA 0x80000000UL
#define HAVEN_SIZE 0x00800000UL /* 8 MB - covers text + BSS stage-2 pool */

/* Partition A (Linux-class): 512 MB
 * PA: [0x80800000, 0xA0800000)  IPA: [0x40000000, 0x60000000) */
#define PART_A_IPA_BASE 0x40000000UL
#define PART_A_PA_BASE (HAVEN_LOAD_PA + HAVEN_SIZE)
#define PART_A_SIZE 0x20000000UL

/* Partition A's UART (PL011 UART0, identity mapped) and its SPI; a plain
 * decimal so partition.c can also print it. */
#define PART_A_UART_BASE QEMU_UART_BASE
#define PART_A_UART_INTID 33

/* Emulated GICv3 for a Linux partition A, at QEMU virt's GIC addresses
 * (docs/methodology/stage2/20261104-linux-partition/partition-a.dts). */
#define PART_A_VGICD_IPA 0x08000000ULL
#define PART_A_VGICR_IPA 0x080a0000ULL

/* Partition B's CPU */
#define PART_B_CPU 2U

/* Partition B (RTOS): 64 MB
 * PA: [0xA0800000, 0xA4800000)  IPA: [0x60000000, 0x64000000)
 * Distinct IPA base from A so cross-partition access faults are detectable. */
#define PART_B_IPA_BASE 0x60000000UL
#define PART_B_PA_BASE (PART_A_PA_BASE + PART_A_SIZE)
#define PART_B_SIZE 0x04000000UL

/* Shared memory: 4 MB */
#define SHARED_PA_BASE (PART_B_PA_BASE + PART_B_SIZE)
#define SHARED_SIZE 0x00400000UL

/* Partition A read-only region: 2 MB, one L2 block
 * PA: [0xA4C00000, 0xA4E00000)  IPA: [0x70000000, 0x70200000)
 * Granted read-only so that write-permission drift in the enforced stage-2
 * tables is visible to read-back (A1). Partition A does not use it yet. */
#define PART_A_RO_IPA_BASE 0x70000000UL
#define PART_A_RO_PA_BASE (SHARED_PA_BASE + SHARED_SIZE)
#define PART_A_RO_SIZE 0x00200000UL

/* DMA device granted to partition A: the PCIe function at 00:01.0 (the first
 * -device on the PCIe root bus, e.g. virtio-rng-pci).  QEMU virt facts, from
 * its device tree (dumpdtb, QEMU 11.1): ECAM at 0x40_1000_0000 (reg of
 * pcie@10000000), 32-bit memory window 0x1000_0000 to 0x3EFF_0000, and
 * iommu-map <0 &smmu 0 0x10000>, so the StreamID is the requester ID
 * (bus << 8 | device << 3 | function).
 *   StreamID 8; config page PA 0x40_1000_8000 (4 KB, this function only,
 *   mapped read-only); memory BAR 4 (where QEMU's virtio-pci keeps its
 *   modern registers) placed by Haven at PA 0x1000_0000 (16 KB);
 *   DMA window: IPA [0x5F000000, 0x5F100000) -> PA [0x9F800000, ...), RW,
 *   the last MiB but one of partition A's RAM (the top MiB holds its stack). */
#define QEMU_PCIE_ECAM_BASE 0x4010000000UL
#define PART_A_DMA_SID 8U
#define PART_A_DMA_CFG_PA (QEMU_PCIE_ECAM_BASE + (1UL << 15))
#define PART_A_DMA_BAR 4U
#define PART_A_DMA_BAR_PA 0x10000000UL
#define PART_A_DMA_BAR_SIZE 0x4000UL
#define PART_A_DMA_IPA (PART_A_IPA_BASE + 0x1F000000UL)
#define PART_A_DMA_PA (PART_A_PA_BASE + 0x1F000000UL)
#define PART_A_DMA_SIZE 0x100000UL

#endif /* HAVEN_PLATFORM_QEMU_VIRT_MEMORY_H */
