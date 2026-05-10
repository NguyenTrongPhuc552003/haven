#include <assert.h>

#include "haven/budget_sched.h"
#include "haven/irq_ownership.h"
#include "haven/stage2.h"

static void run_negative_isolation_flow(void) {
  struct hv_irq_route owner_route = {
      .irq_id = 64,
      .owner_partition_id = 7,
      .target_cpu = 1,
  };

  struct hv_irq_route foreign_route = {
      .irq_id = 64,
      .owner_partition_id = 9,
      .target_cpu = 2,
  };

  struct hv_budget invalid_budget = {
      .partition_id = 7,
      .period_ns = 100000,
      .budget_ns = 200000,
  };

  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_budget_sched_init() == HV_OK);

  assert(hv_stage2_map_partition(NULL) == HV_EINVAL);
  assert(hv_stage2_unmap_partition(0) == HV_EINVAL);

  assert(hv_irq_assign(&owner_route) == HV_OK);
  assert(hv_irq_assign(&foreign_route) == HV_EPERM);
  assert(hv_irq_is_owned_by(owner_route.irq_id, foreign_route.owner_partition_id) == HV_EPERM);
  assert(hv_irq_revoke(owner_route.irq_id, foreign_route.owner_partition_id) == HV_EPERM);
  assert(hv_irq_revoke(owner_route.irq_id, owner_route.owner_partition_id) == HV_OK);

  assert(hv_budget_set(&invalid_budget) == HV_EINVAL);
  assert(hv_budget_consume(0, 1000) == HV_EINVAL);
  assert(hv_budget_reset_period(0) == HV_EINVAL);
}

int main(void) {
  run_negative_isolation_flow();
  return 0;
}
