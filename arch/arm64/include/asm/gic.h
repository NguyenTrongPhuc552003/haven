/* SPDX-License-Identifier: Apache-2.0 */
/* GICv3 MMIO register offsets and bit definitions for the Haven hypervisor.
 * Based on ARM IHI 0069H (GIC Architecture Specification v3/v4). */

#ifndef HAVEN_ASM_GIC_H
#define HAVEN_ASM_GIC_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * GIC Distributor (GICD) register offsets
 * Base address: platform-provided (e.g. 0x08000000 on QEMU virt)
 * ----------------------------------------------------------------------- */

#define GICD_CTLR           0x0000   /* Distributor Control Register */
#define GICD_TYPER          0x0004   /* Interrupt Controller Type Register */
#define GICD_IIDR           0x0008   /* Distributor Implementer Identification */
#define GICD_TYPER2         0x000c   /* Interrupt Controller Type Register 2 */
#define GICD_STATUSR        0x0010   /* Error Reporting Status Register */
#define GICD_SETSPI_NSR     0x0040   /* Set SPI Register (Non-Secure) */
#define GICD_CLRSPI_NSR     0x0048   /* Clear SPI Register (Non-Secure) */
#define GICD_SETSPI_SR      0x0050   /* Set SPI Register (Secure) */
#define GICD_CLRSPI_SR      0x0058   /* Clear SPI Register (Secure) */
#define GICD_IGROUPR(n)     (0x0080 + (n) * 4)   /* Interrupt Group Registers */
#define GICD_ISENABLER(n)   (0x0100 + (n) * 4)   /* Interrupt Set-Enable Registers */
#define GICD_ICENABLER(n)   (0x0180 + (n) * 4)   /* Interrupt Clear-Enable Registers */
#define GICD_ISPENDR(n)     (0x0200 + (n) * 4)   /* Interrupt Set-Pending Registers */
#define GICD_ICPENDR(n)     (0x0280 + (n) * 4)   /* Interrupt Clear-Pending Registers */
#define GICD_ISACTIVER(n)   (0x0300 + (n) * 4)   /* Interrupt Set-Active Registers */
#define GICD_ICACTIVER(n)   (0x0380 + (n) * 4)   /* Interrupt Clear-Active Registers */
#define GICD_IPRIORITYR(n)  (0x0400 + (n) * 4)   /* Interrupt Priority Registers */
#define GICD_ITARGETSR(n)   (0x0800 + (n) * 4)   /* Interrupt Processor Targets (v2 compat) */
#define GICD_ICFGR(n)       (0x0c00 + (n) * 4)   /* Interrupt Configuration Registers */
#define GICD_IGRPMODR(n)    (0x0d00 + (n) * 4)   /* Interrupt Group Modifier Registers */
#define GICD_NSACR(n)       (0x0e00 + (n) * 4)   /* Non-secure Access Control Registers */
#define GICD_SGIR           0x0f00   /* Software Generated Interrupt Register (v2) */
#define GICD_CPENDSGIR(n)   (0x0f10 + (n) * 4)
#define GICD_SPENDSGIR(n)   (0x0f20 + (n) * 4)
#define GICD_INMIR(n)       (0x0f80 + (n) * 4)   /* Non-maskable Interrupt Registers */
#define GICD_IROUTER(n)     (0x6000 + (n) * 8)   /* Interrupt Routing Registers (SPI ≥ 32) */

/* GICD_CTLR bits */
#define GICD_CTLR_ENABLE_G0     (1U << 0)   /* Enable Group 0 */
#define GICD_CTLR_ENABLE_G1NS   (1U << 1)   /* Enable Non-Secure Group 1 */
#define GICD_CTLR_ENABLE_G1S    (1U << 2)   /* Enable Secure Group 1 */
#define GICD_CTLR_ARE_S         (1U << 4)   /* Affinity Routing Enable (Secure) */
#define GICD_CTLR_ARE_NS        (1U << 5)   /* Affinity Routing Enable (NS) */
#define GICD_CTLR_DS            (1U << 6)   /* Disable Security (single-security state) */
#define GICD_CTLR_RWP           (1U << 31)  /* Register Write Pending */

/* GICD_TYPER bits */
#define GICD_TYPER_ITLINES_SHIFT    0
#define GICD_TYPER_ITLINES_MASK     (0x1f)    /* (ITLINES+1)*32 = max INTID+1 */
#define GICD_TYPER_CPUS_SHIFT       5
#define GICD_TYPER_CPUS_MASK        (0x7 << 5)
#define GICD_TYPER_ESPI             (1U << 8)  /* Extended SPI range */
#define GICD_TYPER_NMI              (1U << 9)  /* NMI support */
#define GICD_TYPER_SecurityExtn     (1U << 10) /* Security Extensions */
#define GICD_TYPER_MBIS             (1U << 16) /* MBI support */
#define GICD_TYPER_LPIS             (1U << 17) /* LPI support */
#define GICD_TYPER_DVIS             (1U << 18) /* Direct Virtual LPI injection */
#define GICD_TYPER_ID_BITS_SHIFT    19
#define GICD_TYPER_ID_BITS_MASK     (0x1f << 19)
#define GICD_TYPER_A3V              (1U << 24) /* Affinity Level 3 Values */
#define GICD_TYPER_RSS              (1U << 26) /* Range Selector Support */

/* GICD_IROUTER encoding: affinity routing or 1-of-N targeting */
#define GICD_IROUTER_IRM        (1UL << 31)   /* Interrupt Routing Mode: 1-of-N */
#define GICD_IROUTER_AFF0(x)    ((uint64_t)((x) & 0xff))
#define GICD_IROUTER_AFF1(x)    ((uint64_t)((x) & 0xff) << 8)
#define GICD_IROUTER_AFF2(x)    ((uint64_t)((x) & 0xff) << 16)
#define GICD_IROUTER_AFF3(x)    ((uint64_t)((x) & 0xff) << 32)

/* Encode MPIDR into IROUTER value for direct routing */
#define gicd_irouter_from_mpidr(mpidr) \
        (GICD_IROUTER_AFF0((mpidr) >> 0) | \
         GICD_IROUTER_AFF1((mpidr) >> 8) | \
         GICD_IROUTER_AFF2((mpidr) >> 16) | \
         GICD_IROUTER_AFF3((mpidr) >> 32))

/* -----------------------------------------------------------------------
 * GIC Redistributor (GICR) register offsets
 * Each Redistributor has two 64KB frames: RD_base and SGI_base.
 * Base = GICR_BASE + cpu * GICR_STRIDE
 * ----------------------------------------------------------------------- */

#define GICR_STRIDE         0x20000   /* 128 KiB per redistributor */

/* GICR RD frame (first 64KB) */
#define GICR_CTLR           0x0000   /* Redistributor Control Register */
#define GICR_IIDR           0x0004   /* Implementer Identification Register */
#define GICR_TYPER          0x0008   /* Redistributor Type Register (64-bit) */
#define GICR_STATUSR        0x0010   /* Error Reporting Status Register */
#define GICR_WAKER          0x0014   /* Redistributor Wake Register */
#define GICR_MPAMIDR        0x0018   /* Report maximum PARTID Register */
#define GICR_PARTID         0x001c   /* Set PARTID and PMG Register */
#define GICR_PROPBASER      0x0070   /* LPI Configuration Table Base Address */
#define GICR_PENDBASER      0x0078   /* LPI Pending Table Base Address */

/* GICR SGI frame (second 64KB, offset +0x10000) */
#define GICR_SGI_OFFSET     0x10000
#define GICR_IGROUPR0       (GICR_SGI_OFFSET + 0x0080)  /* SGI/PPI Group */
#define GICR_ISENABLER0     (GICR_SGI_OFFSET + 0x0100)  /* SGI/PPI Set-Enable */
#define GICR_ICENABLER0     (GICR_SGI_OFFSET + 0x0180)  /* SGI/PPI Clear-Enable */
#define GICR_ISPENDR0       (GICR_SGI_OFFSET + 0x0200)  /* SGI/PPI Set-Pending */
#define GICR_ICPENDR0       (GICR_SGI_OFFSET + 0x0280)  /* SGI/PPI Clear-Pending */
#define GICR_ISACTIVER0     (GICR_SGI_OFFSET + 0x0300)  /* SGI/PPI Set-Active */
#define GICR_ICACTIVER0     (GICR_SGI_OFFSET + 0x0380)  /* SGI/PPI Clear-Active */
#define GICR_IPRIORITYR(n)  (GICR_SGI_OFFSET + 0x0400 + (n) * 4)
#define GICR_ICFGR0         (GICR_SGI_OFFSET + 0x0c00)  /* SGI Configuration */
#define GICR_ICFGR1         (GICR_SGI_OFFSET + 0x0c04)  /* PPI Configuration */
#define GICR_IGRPMODR0      (GICR_SGI_OFFSET + 0x0d00)  /* SGI/PPI Group Modifier */
#define GICR_NSACR          (GICR_SGI_OFFSET + 0x0e00)  /* Non-Secure Access Control */
#define GICR_INMIR0         (GICR_SGI_OFFSET + 0x0f80)  /* NMI Registers */

/* GICR_CTLR bits */
#define GICR_CTLR_ENABLE_LPIS   (1U << 0)   /* Enable LPIs */
#define GICR_CTLR_CES           (1U << 1)   /* Controls Clr/SetEnable */
#define GICR_CTLR_IR            (1U << 2)   /* Inactive Read */
#define GICR_CTLR_RWP           (1U << 3)   /* Register Write Pending */
#define GICR_CTLR_DPG0          (1U << 24)  /* Disable Group 0 */
#define GICR_CTLR_DPG1NS        (1U << 25)  /* Disable Group 1 Non-Secure */
#define GICR_CTLR_DPG1S         (1U << 26)  /* Disable Group 1 Secure */
#define GICR_CTLR_UWP           (1U << 31)  /* Upstream Write Pending */

/* GICR_WAKER bits */
#define GICR_WAKER_PROCESSOR_SLEEP  (1U << 1)   /* Set to power down */
#define GICR_WAKER_CHILDREN_ASLEEP  (1U << 2)   /* Indicates powered down */

/* GICR_TYPER bits */
#define GICR_TYPER_PLPIS        (1UL << 0)   /* Physical LPIs supported */
#define GICR_TYPER_VLPIS        (1UL << 1)   /* Virtual LPIs supported */
#define GICR_TYPER_DIRECTLPI    (1UL << 3)   /* Direct LPI injection */
#define GICR_TYPER_LAST         (1UL << 4)   /* Last redistributor in set */
#define GICR_TYPER_DPGS         (1UL << 5)   /* GICR_CTLR.DPG* supported */
#define GICR_TYPER_PROCNUM_SHIFT 8
#define GICR_TYPER_PROCNUM_MASK  (0xffff << 8)  /* Processor Number */
#define GICR_TYPER_AFF_SHIFT    32            /* Affinity value */

/* -----------------------------------------------------------------------
 * GICv3 interrupt ID ranges
 * ----------------------------------------------------------------------- */

#define GIC_SGI_BASE        0      /* Software Generated Interrupts: 0–15 */
#define GIC_SGI_MAX         15
#define GIC_PPI_BASE        16     /* Private Peripheral Interrupts: 16–31 */
#define GIC_PPI_MAX         31
#define GIC_SPI_BASE        32     /* Shared Peripheral Interrupts: 32–1019 */
#define GIC_SPI_MAX         1019
#define GIC_INTID_SPECIAL   1020   /* Special INTID range: 1020–1023 */
#define GIC_INTID_SPURIOUS  1023

/* -----------------------------------------------------------------------
 * Priority encoding
 * GICv3 uses 8-bit priority; high values = low priority.
 * Grouping: Grp0 must have higher priority (lower number) than Grp1.
 * ----------------------------------------------------------------------- */

#define GIC_PRIO_HIGHEST        0x00   /* Highest priority */
#define GIC_PRIO_HYPERVISOR     0x40   /* Haven EL2 handlers */
#define GIC_PRIO_RTOS_HIGH      0x60   /* High-criticality RTOS partition */
#define GIC_PRIO_RTOS_LOW       0x80   /* Low-criticality RTOS */
#define GIC_PRIO_LINUX          0xa0   /* Linux partition */
#define GIC_PRIO_LOWEST         0xf0   /* Lowest priority */

/* Priority mask for non-maskable interrupts (Grp0) */
#define GIC_PMASK_ALL           0xff   /* No interrupts masked */
#define GIC_PMASK_RTOS          0x80   /* Block only Linux-priority IRQs */

/* -----------------------------------------------------------------------
 * MMIO accessor helpers
 * ----------------------------------------------------------------------- */

static inline uint32_t gic_read32(uintptr_t base, uint32_t offset)
{
        return *(volatile uint32_t *)(base + offset);
}

static inline void gic_write32(uintptr_t base, uint32_t offset, uint32_t val)
{
        *(volatile uint32_t *)(base + offset) = val;
}

static inline uint64_t gic_read64(uintptr_t base, uint32_t offset)
{
        return *(volatile uint64_t *)(base + offset);
}

static inline void gic_write64(uintptr_t base, uint32_t offset, uint64_t val)
{
        *(volatile uint64_t *)(base + offset) = val;
}

/* Poll GICD_CTLR.RWP until clear (distributor write committed) */
static inline void gicd_wait_rwp(uintptr_t gicd_base)
{
        while (gic_read32(gicd_base, GICD_CTLR) & GICD_CTLR_RWP)
                __asm__ volatile("yield");
}

/* Poll GICR_CTLR.RWP until clear (redistributor write committed) */
static inline void gicr_wait_rwp(uintptr_t gicr_base)
{
        while (gic_read32(gicr_base, GICR_CTLR) & GICR_CTLR_RWP)
                __asm__ volatile("yield");
}

#endif /* HAVEN_ASM_GIC_H */
