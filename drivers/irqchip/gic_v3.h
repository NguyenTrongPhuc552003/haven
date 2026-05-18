/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/irqchip/gic_v3.h - Public interface for the Haven GICv3 driver.
 *
 * The core isolation layer (src/core/irq/ownership.c) calls these functions
 * after validating partition ownership, so the driver can assume all policy
 * checks have passed.
 */

#ifndef HAVEN_DRIVERS_GIC_V3_H
#define HAVEN_DRIVERS_GIC_V3_H

#include <stdint.h>
#include <haven/types.h>

/*
 * gic_v3_init - initialize distributor + all redistributors.
 * Must be called before any IRQ assignment.
 */
hv_status_t gic_v3_init(uintptr_t gicd_pa, uintptr_t gicr_pa, uint32_t nr_cpus);

/*
 * gic_v3_configure_irq - set priority and trigger mode for an SPI.
 * @priority: GIC priority byte (0x00 = highest, 0xff = lowest)
 * @edge:     1 = edge-triggered, 0 = level-sensitive
 */
hv_status_t gic_v3_configure_irq(uint32_t irq, uint8_t priority, int edge);

/* Enable an IRQ at the distributor/redistributor level. */
hv_status_t gic_v3_enable_irq(uint32_t irq);

/* Disable an IRQ - used by hv_irq_revoke() to enforce isolation. */
hv_status_t gic_v3_disable_irq(uint32_t irq);

/*
 * gic_v3_route_irq - affinity-route SPI to a physical CPU.
 * @mpidr: target CPU MPIDR (Aff3.Aff2.Aff1.Aff0 format).
 */
hv_status_t gic_v3_route_irq(uint32_t irq, uint64_t mpidr);

/* Acknowledge an IRQ (reads ICC_IAR1_EL1). Returns IAR value. */
uint32_t gic_v3_ack_irq(void);

/* Signal end-of-interrupt (writes ICC_EOIR1_EL1). */
void gic_v3_eoi_irq(uint32_t iar);

/* Return detected SPI count (0 if not initialized). */
uint32_t gic_v3_nr_irqs(void);

#endif /* HAVEN_DRIVERS_GIC_V3_H */
