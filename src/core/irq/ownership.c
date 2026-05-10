#include "haven/irq_ownership.h"

#define HV_MAX_IRQ_ID 1024U

static hv_u32 irq_owner_map[HV_MAX_IRQ_ID];

hv_status_t hv_irq_owner_init(void) {
  hv_u32 i;

  for (i = 0U; i < HV_MAX_IRQ_ID; ++i) {
    irq_owner_map[i] = 0U;
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

  if (irq_owner_map[route->irq_id] != 0U &&
      irq_owner_map[route->irq_id] != route->owner_partition_id) {
    return HV_EPERM;
  }

  irq_owner_map[route->irq_id] = route->owner_partition_id;

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

  irq_owner_map[irq_id] = 0U;

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
