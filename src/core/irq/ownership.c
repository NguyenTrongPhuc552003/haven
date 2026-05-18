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

#define HV_MAX_IRQ_ID       1024U
#define HV_MAX_TARGET_CPU   256U

static hv_u32 irq_owner_map[HV_MAX_IRQ_ID];
static hv_u32 irq_target_cpu_map[HV_MAX_IRQ_ID];

hv_status_t hv_irq_owner_init(void) {
  hv_u32 i;

  for (i = 0U; i < HV_MAX_IRQ_ID; ++i) {
    irq_owner_map[i] = 0U;
    irq_target_cpu_map[i] = 0U;
  }

  return HV_OK;
}

hv_status_t hv_irq_assign(const struct hv_irq_route *route) {
  if (route == NULL || route->irq_id == 0U || route->owner_partition_id == 0U) {
    return HV_EINVAL;
  }

  if (route->irq_id >= HV_MAX_IRQ_ID) {
    return HV_ENOSPC;
  }

  if (route->target_cpu >= HV_MAX_TARGET_CPU) {
    return HV_ENOSPC;
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

hv_status_t hv_irq_revoke(hv_u32 irq_id, hv_u32 owner_partition_id) {
  if (irq_id == 0U || owner_partition_id == 0U) {
    return HV_EINVAL;
  }

  if (irq_id >= HV_MAX_IRQ_ID) {
    return HV_ENOSPC;
  }

  if (irq_owner_map[irq_id] != owner_partition_id) {
    return HV_EPERM;
  }

#ifdef HAVEN_ARCH_ARM64
  /* Disable IRQ in hardware before clearing ownership table —
   * fail-closed: the IRQ cannot fire even momentarily after revocation. */
  gic_v3_disable_irq(irq_id);
#endif

  irq_owner_map[irq_id] = 0U;
  irq_target_cpu_map[irq_id] = 0U;

  return HV_OK;
}

hv_status_t hv_irq_is_owned_by(hv_u32 irq_id, hv_u32 partition_id) {
  if (irq_id == 0U || partition_id == 0U) {
    return HV_EINVAL;
  }

  if (irq_id >= HV_MAX_IRQ_ID) {
    return HV_ENOSPC;
  }

  if (irq_owner_map[irq_id] != partition_id) {
    return HV_EPERM;
  }

  return HV_OK;
}
