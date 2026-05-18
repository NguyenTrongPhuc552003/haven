#ifndef HAVEN_PLATFORM_QEMU_VIRT_MEMORY_H
#define HAVEN_PLATFORM_QEMU_VIRT_MEMORY_H

/* QEMU virt machine (aarch64) memory map */
#define QEMU_UART_BASE          0x09000000UL
#define QEMU_GIC_DIST_BASE      0x08000000UL
#define QEMU_GIC_REDIST_BASE    0x080A0000UL
#define QEMU_SMMU_BASE          0x09050000UL  /* SMMUv3 for QEMU virt */
#define QEMU_TIMER_FREQ         62500000UL    /* 62.5 MHz */
#define QEMU_NR_CPUS            4

/* DRAM layout: Haven loads at 0x40000000 */
#define QEMU_DRAM_BASE          0x40000000UL
#define QEMU_DRAM_SIZE          0x80000000UL  /* 2 GiB */

/* Haven hypervisor default load address (matches linker HV_LOAD_ADDR). */
#define HAVEN_LOAD_PA           0x80000000UL
#define HAVEN_SIZE              0x00400000UL  /* 4 MB */

/* Partition A (Linux-class): 512 MB */
#define PART_A_IPA_BASE         0x40000000UL
#define PART_A_PA_BASE          (HAVEN_LOAD_PA + HAVEN_SIZE)
#define PART_A_SIZE             0x20000000UL

/* Partition B (RTOS): 64 MB */
#define PART_B_IPA_BASE         0x40000000UL
#define PART_B_PA_BASE          (PART_A_PA_BASE + PART_A_SIZE)
#define PART_B_SIZE             0x04000000UL

/* Shared memory: 4 MB */
#define SHARED_PA_BASE          (PART_B_PA_BASE + PART_B_SIZE)
#define SHARED_SIZE             0x00400000UL

#endif /* HAVEN_PLATFORM_QEMU_VIRT_MEMORY_H */
