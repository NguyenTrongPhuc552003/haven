#ifndef HAVEN_IRQ_OWNERSHIP_H
#define HAVEN_IRQ_OWNERSHIP_H

#include "types.h"

struct hv_irq_route {
  hv_u32 irq_id;
  hv_u32 owner_partition_id;
  hv_u32 target_cpu;
};

hv_status_t hv_irq_owner_init(void);
hv_status_t hv_irq_assign(const struct hv_irq_route *route);
hv_status_t hv_irq_revoke(hv_u32 irq_id, hv_u32 owner_partition_id);
hv_status_t hv_irq_is_owned_by(hv_u32 irq_id, hv_u32 partition_id);

#endif
