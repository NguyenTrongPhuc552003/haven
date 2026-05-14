/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/irqchip/gic_v3.c — GICv3 interrupt controller driver.
 *
 * Programs the ARM GICv3 GICD (Distributor) and GICR (Redistributor) MMIO
 * registers to implement IRQ ownership isolation between Haven partitions.
 *
 * Design:
 *   - The hypervisor owns Distributor configuration at EL2.
 *   - Each SPI (IRQ ≥ 32) is affinity-routed to exactly one physical CPU.
 *   - IRQ ownership revocation disables the IRQ in GICD and clears its
 *     affinity routing — the IRQ cannot fire until explicitly reassigned.
 *   - PPIs (IRQ 16-31) are per-CPU and managed through GICR_SGI_OFFSET frame.
 *   - SGIs (IRQ 0-15) are always available to all CPUs; Haven does not
 *     restrict SGIs between partitions.
 *
 * References:
 *   ARM IHI 0069H — GIC Architecture Specification v3 and v4
 */

#include <stdint.h>
#include <haven/types.h>
#include <asm/gic.h>

/* -----------------------------------------------------------------------
 * Driver state
 * ----------------------------------------------------------------------- */

static struct {
        uintptr_t gicd_base;
        uintptr_t gicr_base;
        uint32_t  nr_cpus;
        uint32_t  nr_irqs;
        int       initialized;
} gic;

/* -----------------------------------------------------------------------
 * Redistributor per-CPU base address
 * ----------------------------------------------------------------------- */

static inline uintptr_t gicr_cpu_base(uint32_t cpu)
{
        return gic.gicr_base + (uintptr_t)cpu * GICR_STRIDE;
}

/* -----------------------------------------------------------------------
 * GICD Distributor initialization
 * ----------------------------------------------------------------------- */

static void gicd_init(void)
{
        uint32_t typer, itlines;

        /* Disable distributor while configuring */
        gic_write32(gic.gicd_base, GICD_CTLR, 0);
        gicd_wait_rwp(gic.gicd_base);

        /* Detect actual IRQ count from TYPER */
        typer   = gic_read32(gic.gicd_base, GICD_TYPER);
        itlines = (typer & GICD_TYPER_ITLINES_MASK) >> GICD_TYPER_ITLINES_SHIFT;
        gic.nr_irqs = (itlines + 1) * 32;
        if (gic.nr_irqs > 1020)
                gic.nr_irqs = 1020;

        /* Put all SPIs in Group 1 Non-Secure */
        for (uint32_t i = 1; i < (gic.nr_irqs / 32); i++)
                gic_write32(gic.gicd_base, GICD_IGROUPR(i), 0xffffffff);

        /* Disable, clear pending/active for all SPIs */
        for (uint32_t i = 1; i < (gic.nr_irqs / 32); i++) {
                gic_write32(gic.gicd_base, GICD_ICENABLER(i), 0xffffffff);
                gic_write32(gic.gicd_base, GICD_ICPENDR(i),   0xffffffff);
                gic_write32(gic.gicd_base, GICD_ICACTIVER(i), 0xffffffff);
        }

        /* Default priority: Linux-level for all SPIs */
        for (uint32_t i = 8; i < (gic.nr_irqs / 4); i++)
                gic_write32(gic.gicd_base, GICD_IPRIORITYR(i),
                            GIC_PRIO_LINUX * 0x01010101U);

        /* Route all SPIs as 1-of-N until explicitly assigned */
        for (uint32_t i = GIC_SPI_BASE; i < gic.nr_irqs; i++)
                gic_write64(gic.gicd_base, GICD_IROUTER(i), GICD_IROUTER_IRM);

        /* Enable: ARE_NS + G1NS + G0 */
        gic_write32(gic.gicd_base, GICD_CTLR,
                    GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1NS | GICD_CTLR_ENABLE_G0);
        gicd_wait_rwp(gic.gicd_base);
}

/* -----------------------------------------------------------------------
 * GICR Redistributor initialization (per CPU)
 * ----------------------------------------------------------------------- */

static void gicr_cpu_init(uint32_t cpu)
{
        uintptr_t base = gicr_cpu_base(cpu);
        uint32_t waker;

        /* Wake the redistributor: clear ProcessorSleep */
        waker  = gic_read32(base, GICR_WAKER);
        waker &= ~GICR_WAKER_PROCESSOR_SLEEP;
        gic_write32(base, GICR_WAKER, waker);

        /* Wait for ChildrenAsleep to clear */
        while (gic_read32(base, GICR_WAKER) & GICR_WAKER_CHILDREN_ASLEEP)
                __asm__ volatile("yield");

        /* All SGIs/PPIs → Group 1 NS */
        gic_write32(base, GICR_IGROUPR0, 0xffffffff);

        /* SGIs enabled by default; PPIs disabled (assigned per partition) */
        gic_write32(base, GICR_ICENABLER0, 0xffff0000U);
        gic_write32(base, GICR_ISENABLER0, 0x0000ffffU);

        /* Default priority: Linux-level */
        for (uint32_t i = 0; i < 8; i++)
                gic_write32(base, GICR_IPRIORITYR(i), GIC_PRIO_LINUX * 0x01010101U);

        gicr_wait_rwp(base);
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

hv_status_t gic_v3_init(uintptr_t gicd_pa, uintptr_t gicr_pa, uint32_t nr_cpus)
{
        if (!gicd_pa || !gicr_pa || nr_cpus == 0 || nr_cpus > 16)
                return HV_EINVAL;

        gic.gicd_base   = gicd_pa;
        gic.gicr_base   = gicr_pa;
        gic.nr_cpus     = nr_cpus;
        gic.initialized = 0;

        gicd_init();
        for (uint32_t i = 0; i < nr_cpus; i++)
                gicr_cpu_init(i);

        gic.initialized = 1;
        return HV_OK;
}

hv_status_t gic_v3_configure_irq(uint32_t irq, uint8_t priority, int edge)
{
        uint32_t reg, shift, val;

        if (!gic.initialized || irq < GIC_SPI_BASE || irq >= gic.nr_irqs)
                return HV_EINVAL;

        /* Priority: 1 byte per IRQ */
        reg   = GICD_IPRIORITYR(irq / 4);
        shift = (irq % 4) * 8;
        val   = gic_read32(gic.gicd_base, reg);
        val  &= ~(0xffU << shift);
        val  |= ((uint32_t)priority << shift);
        gic_write32(gic.gicd_base, reg, val);

        /* Edge/level: 2 bits per IRQ */
        reg   = GICD_ICFGR(irq / 16);
        shift = (irq % 16) * 2;
        val   = gic_read32(gic.gicd_base, reg);
        if (edge)
                val |=  (2U << shift);
        else
                val &= ~(2U << shift);
        gic_write32(gic.gicd_base, reg, val);

        return HV_OK;
}

hv_status_t gic_v3_enable_irq(uint32_t irq)
{
        if (!gic.initialized || irq >= gic.nr_irqs)
                return HV_EINVAL;

        if (irq < GIC_SPI_BASE) {
                /* PPI: enable via boot CPU redistributor */
                gic_write32(gicr_cpu_base(0), GICR_ISENABLER0, 1U << irq);
        } else {
                gic_write32(gic.gicd_base, GICD_ISENABLER(irq / 32),
                            1U << (irq % 32));
        }
        return HV_OK;
}

hv_status_t gic_v3_disable_irq(uint32_t irq)
{
        if (!gic.initialized || irq >= gic.nr_irqs)
                return HV_EINVAL;

        if (irq < GIC_SPI_BASE) {
                gic_write32(gicr_cpu_base(0), GICR_ICENABLER0, 1U << irq);
        } else {
                gic_write32(gic.gicd_base, GICD_ICENABLER(irq / 32),
                            1U << (irq % 32));
        }
        gicd_wait_rwp(gic.gicd_base);
        return HV_OK;
}

hv_status_t gic_v3_route_irq(uint32_t irq, uint64_t mpidr)
{
        if (!gic.initialized || irq < GIC_SPI_BASE || irq >= gic.nr_irqs)
                return HV_EINVAL;

        gic_write64(gic.gicd_base, GICD_IROUTER(irq),
                    gicd_irouter_from_mpidr(mpidr));
        return HV_OK;
}

uint32_t gic_v3_ack_irq(void)
{
        uint32_t iar;
        __asm__ volatile("mrs %0, icc_iar1_el1" : "=r"(iar));
        return iar;
}

void gic_v3_eoi_irq(uint32_t iar)
{
        __asm__ volatile("msr icc_eoir1_el1, %0" :: "r"((uint64_t)iar));
        __asm__ volatile("isb");
}

uint32_t gic_v3_nr_irqs(void)
{
        return gic.initialized ? gic.nr_irqs : 0;
}
