#include "haven/irq_ownership.h"

hv_status_t hv_irq_owner_init(void) { return HV_OK; }

hv_status_t hv_irq_assign(const struct hv_irq_route *route) {
  if (route == NULL || route->irq_id == 0U) {
    return HV_EINVAL;
  }

  return HV_OK;
}

hv_status_t hv_irq_revoke(hv_u32 irq_id, hv_u32 owner_partition_id) {
  if (irq_id == 0U || owner_partition_id == 0U) {
    return HV_EINVAL;
  }

  return HV_OK;
}

hv_status_t hv_irq_is_owned_by(hv_u32 irq_id, hv_u32 partition_id) {
  if (irq_id == 0U || partition_id == 0U) {
    return HV_EINVAL;
  }

  return HV_OK;
}
