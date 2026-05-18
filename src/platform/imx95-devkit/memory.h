/* SPDX-License-Identifier: Apache-2.0 */
/* i.MX95 Development Kit physical memory map.
 *
 * All addresses from:
 *   NXP i.MX95 Reference Manual, Rev. 0, 2023
 *   Chapter 2 "System Memory Map"
 *
 * This header is the single source of truth for address and size constants
 * used throughout the i.MX95 platform port.  platform.c and any board-level
 * test fixtures must include this file instead of embedding literals.
 */

#ifndef HAVEN_PLATFORM_IMX95_MEMORY_H
#define HAVEN_PLATFORM_IMX95_MEMORY_H

/* -----------------------------------------------------------------------
 * Debug UART
 * LPUART1 (console, initialized by U-Boot before handing off to Haven)
 * ----------------------------------------------------------------------- */
#define IMX95_UART1_BASE        0x44380000UL

/* -----------------------------------------------------------------------
 * Interrupt controller — GIC-700 (GICv3)
 * ----------------------------------------------------------------------- */
#define IMX95_GIC_DIST_BASE     0x48000000UL
#define IMX95_GIC_REDIST_BASE   0x48040000UL  /* Stride: 0x20000 per CPU */
#define IMX95_GIC_REDIST_STRIDE 0x00020000UL

/* -----------------------------------------------------------------------
 * IOMMU — SMMUv3
 * ----------------------------------------------------------------------- */
#define IMX95_SMMU_BASE         0x49800000UL

/* -----------------------------------------------------------------------
 * Generic Timer
 * 24 MHz crystal oscillator is the counter reference on i.MX95.
 * ATF programs CNTFRQ_EL0 to this value before entering EL2.
 * ----------------------------------------------------------------------- */
#define IMX95_TIMER_FREQ        24000000UL

/* -----------------------------------------------------------------------
 * CPU topology
 * 6x Cortex-A55 cores in a single cluster; Cortex-M7 subsystems run
 * outside this count (managed separately by the Cortex-M7 RTOS).
 * ----------------------------------------------------------------------- */
#define IMX95_NR_CPUS           6

/* -----------------------------------------------------------------------
 * DRAM
 * Standard Dev Kit configuration: 2 GiB LPDDR5 at 0x80000000.
 * ----------------------------------------------------------------------- */
#define IMX95_DRAM_BASE         0x80000000UL
#define IMX95_DRAM_SIZE         0x80000000UL   /* 2 GiB */

/* -----------------------------------------------------------------------
 * Haven hypervisor image
 * Loaded at the very start of DRAM; 8 MB reserved — larger than the QEMU
 * target to leave margin for real-hardware debug instrumentation.
 * ----------------------------------------------------------------------- */
#define HAVEN_IMX95_LOAD_PA     0x80000000UL
#define HAVEN_IMX95_SIZE        0x00800000UL   /* 8 MiB */

/* -----------------------------------------------------------------------
 * Partition A — Linux guest
 * 1 GiB, Cortex-A55 cores 0–3.  IPA base mirrors guest physical view.
 * ----------------------------------------------------------------------- */
#define IMX95_PART_A_IPA_BASE   0x80000000UL
#define IMX95_PART_A_PA_BASE    (HAVEN_IMX95_LOAD_PA + HAVEN_IMX95_SIZE)
#define IMX95_PART_A_SIZE       0x40000000UL   /* 1 GiB */

/* -----------------------------------------------------------------------
 * Partition B — RTOS guest
 * 128 MiB, Cortex-A55 core 4.  Isolated from Partition A at stage-2.
 * ----------------------------------------------------------------------- */
#define IMX95_PART_B_IPA_BASE   0x80000000UL
#define IMX95_PART_B_PA_BASE    (IMX95_PART_A_PA_BASE + IMX95_PART_A_SIZE)
#define IMX95_PART_B_SIZE       0x08000000UL   /* 128 MiB */

/* -----------------------------------------------------------------------
 * Shared memory region
 * 4 MiB mapped read-write into both partitions for inter-partition IPC.
 * Stage-2 entries for this region must be configured with shared attributes.
 * ----------------------------------------------------------------------- */
#define IMX95_SHARED_PA_BASE    (IMX95_PART_B_PA_BASE + IMX95_PART_B_SIZE)
#define IMX95_SHARED_SIZE       0x00400000UL   /* 4 MiB */

/* -----------------------------------------------------------------------
 * Cortex-M7 tightly-coupled memories
 * Not used by the A55 hypervisor; included for completeness and to support
 * future cross-core DMA policy enforcement via the SMMU.
 * ----------------------------------------------------------------------- */
#define IMX95_CM7_ITCM_BASE     0x00000000UL
#define IMX95_CM7_DTCM_BASE     0x20000000UL
#define IMX95_CM7_ITCM_SIZE     0x00040000UL   /* 256 KiB */
#define IMX95_CM7_DTCM_SIZE     0x00040000UL   /* 256 KiB */

#endif /* HAVEN_PLATFORM_IMX95_MEMORY_H */
