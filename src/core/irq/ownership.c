#include <haven/irq_ownership.h>
#ifdef HAVEN_ARCH_ARM64
#include "drivers/irqchip/gic_v3.h"
#include <asm/sysregs.h>

/* Compute MPIDR from logical CPU index.
 * For single-cluster SoCs: MPIDR Aff0 = cpu_id, Aff1..3 = 0. */
static inline uint64_t cpu_to_mpidr(uint32_t cpu)
{
	return (uint64_t)cpu;
}
#endif

#define HV_MAX_IRQ_ID 1024U
#define HV_MAX_TARGET_CPU 256U

/* INTID classes (GICv3): SGIs 0-15 are Haven's own; PPIs 16-31 are private
 * to one CPU, so a PPI is owned per (INTID, CPU); SPIs from 32 are shared
 * and owned per INTID with one target CPU. */
#define HV_IRQ_PPI_BASE 16U
#define HV_IRQ_SPI_BASE 32U
#define HV_IRQ_NR_PPI 16U
#define HV_MAX_PPI_CPU 16U /* GICv3 driver limit (gic_v3_init) */

static hv_u32 irq_owner_map[HV_MAX_IRQ_ID];
static hv_u32 irq_target_cpu_map[HV_MAX_IRQ_ID];
static hv_u32 ppi_owner_map[HV_MAX_PPI_CPU][HV_IRQ_NR_PPI];

static int is_ppi(hv_u32 irq_id)
{
	return irq_id >= HV_IRQ_PPI_BASE && irq_id < HV_IRQ_SPI_BASE;
}

hv_status_t hv_irq_owner_init(void)
{
	hv_u32 i;
	hv_u32 j;

	for (i = 0U; i < HV_MAX_IRQ_ID; ++i) {
		irq_owner_map[i] = 0U;
		irq_target_cpu_map[i] = 0U;
	}
	for (i = 0U; i < HV_MAX_PPI_CPU; ++i) {
		for (j = 0U; j < HV_IRQ_NR_PPI; ++j) {
			ppi_owner_map[i][j] = 0U;
		}
	}

	return HV_OK;
}

static hv_status_t ppi_assign(const struct hv_irq_route *route)
{
	hv_u32 *slot;

	if (route->target_cpu >= HV_MAX_PPI_CPU) {
		return HV_ENOSPC;
	}
	slot = &ppi_owner_map[route->target_cpu]
			     [route->irq_id - HV_IRQ_PPI_BASE];
	if (*slot != 0U && *slot != route->owner_partition_id) {
		return HV_EPERM;
	}
#ifdef HAVEN_ARCH_ARM64
	{
		hv_status_t st =
			gic_v3_enable_ppi(route->target_cpu, route->irq_id);
		if (st != HV_OK) {
			return st;
		}
	}
#endif
	*slot = route->owner_partition_id;
	return HV_OK;
}

hv_status_t hv_irq_assign(const struct hv_irq_route *route)
{
	if (route == NULL || route->irq_id == 0U ||
	    route->owner_partition_id == 0U) {
		return HV_EINVAL;
	}

	if (route->irq_id >= HV_MAX_IRQ_ID) {
		return HV_ENOSPC;
	}

	if (route->target_cpu >= HV_MAX_TARGET_CPU) {
		return HV_ENOSPC;
	}

	if (route->irq_id < HV_IRQ_PPI_BASE) {
		return HV_EINVAL; /* SGI: Haven's own */
	}

	if (is_ppi(route->irq_id)) {
		return ppi_assign(route);
	}

	if (irq_owner_map[route->irq_id] != 0U &&
	    irq_owner_map[route->irq_id] != route->owner_partition_id) {
		return HV_EPERM;
	}

	if (irq_owner_map[route->irq_id] == route->owner_partition_id &&
	    irq_target_cpu_map[route->irq_id] != route->target_cpu) {
		return HV_EPERM;
	}

#ifdef HAVEN_ARCH_ARM64
	/* Wire hardware: route IRQ to target CPU via GICv3 affinity routing */
	{
		uint64_t mpidr = cpu_to_mpidr(route->target_cpu);
		hv_status_t st = gic_v3_route_irq(route->irq_id, mpidr);
		if (st != HV_OK) {
			return st;
		}
		gic_v3_enable_irq(route->irq_id);
	}
#endif

	/* Update ownership table only after hardware route succeeds. */
	irq_owner_map[route->irq_id] = route->owner_partition_id;
	irq_target_cpu_map[route->irq_id] = route->target_cpu;

	return HV_OK;
}

hv_status_t hv_irq_revoke(hv_u32 irq_id, hv_u32 owner_partition_id)
{
	if (irq_id == 0U || owner_partition_id == 0U) {
		return HV_EINVAL;
	}

	if (irq_id >= HV_MAX_IRQ_ID) {
		return HV_ENOSPC;
	}

	if (is_ppi(irq_id)) {
		/* Revoke the owner's PPI on every CPU where it holds it. */
		hv_status_t st = HV_EPERM;
		hv_u32 cpu;

		for (cpu = 0U; cpu < HV_MAX_PPI_CPU; ++cpu) {
			hv_u32 *slot =
				&ppi_owner_map[cpu][irq_id - HV_IRQ_PPI_BASE];
			if (*slot != owner_partition_id) {
				continue;
			}
#ifdef HAVEN_ARCH_ARM64
			gic_v3_disable_ppi(cpu, irq_id);
#endif
			*slot = 0U;
			st = HV_OK;
		}
		return st;
	}

	if (irq_owner_map[irq_id] != owner_partition_id) {
		return HV_EPERM;
	}

#ifdef HAVEN_ARCH_ARM64
	/* Disable IRQ in hardware before clearing ownership table -
   * fail-closed: the IRQ cannot fire even momentarily after revocation. */
	gic_v3_disable_irq(irq_id);
#endif

	irq_owner_map[irq_id] = 0U;
	irq_target_cpu_map[irq_id] = 0U;

	return HV_OK;
}

hv_status_t hv_irq_is_owned_by(hv_u32 irq_id, hv_u32 partition_id)
{
	if (irq_id == 0U || partition_id == 0U) {
		return HV_EINVAL;
	}

	if (irq_id >= HV_MAX_IRQ_ID) {
		return HV_ENOSPC;
	}

	if (is_ppi(irq_id)) {
		hv_u32 cpu;

		for (cpu = 0U; cpu < HV_MAX_PPI_CPU; ++cpu) {
			if (ppi_owner_map[cpu][irq_id - HV_IRQ_PPI_BASE] ==
			    partition_id) {
				return HV_OK;
			}
		}
		return HV_EPERM;
	}

	if (irq_owner_map[irq_id] != partition_id) {
		return HV_EPERM;
	}

	return HV_OK;
}

hv_u32 hv_irq_owner(hv_u32 irq_id, hv_u32 *target_cpu)
{
	if (irq_id == 0U || irq_id >= HV_MAX_IRQ_ID || is_ppi(irq_id)) {
		return 0U; /* PPIs: hv_irq_owner_on */
	}

	if (target_cpu != NULL) {
		*target_cpu = irq_target_cpu_map[irq_id];
	}
	return irq_owner_map[irq_id];
}

hv_u32 hv_irq_owner_on(hv_u32 irq_id, hv_u32 cpu)
{
	if (is_ppi(irq_id)) {
		if (cpu >= HV_MAX_PPI_CPU) {
			return 0U;
		}
		return ppi_owner_map[cpu][irq_id - HV_IRQ_PPI_BASE];
	}
	return hv_irq_owner(irq_id, NULL);
}
