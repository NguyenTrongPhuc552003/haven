#include <assert.h>

#include "haven/budget_sched.h"
#include "haven/irq_ownership.h"
#include "haven/stage2.h"

static void run_isolation_flow(void) {
  struct hv_mem_region regions[] = {
      {.ipa_base = 0x40000000,
       .pa_base = 0x40000000,
       .size = 0x2000,
       .attrs = 0},
      {.ipa_base = 0x50000000,
       .pa_base = 0x50000000,
       .size = 0x2000,
       .attrs = 0},
  };

  struct hv_partition_mem partition = {
      .partition_id = 1,
      .regions = regions,
      .region_count = (hv_u32)(sizeof(regions) / sizeof(regions[0])),
  };

  struct hv_irq_route irq = {
      .irq_id = 48,
      .owner_partition_id = 1,
      .target_cpu = 2,
  };

  struct hv_budget budget = {
      .partition_id = 1,
      .period_ns = 1000000,
      .budget_ns = 300000,
  };

  assert(hv_stage2_init() == HV_OK);
  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_budget_sched_init() == HV_OK);

  assert(hv_stage2_map_partition(&partition) == HV_OK);
  assert(hv_irq_assign(&irq) == HV_OK);
  assert(hv_budget_set(&budget) == HV_OK);

  assert(hv_budget_consume(partition.partition_id, 10000) == HV_OK);
  assert(hv_budget_reset_period(1000000) == HV_OK);

  assert(hv_irq_revoke(irq.irq_id, irq.owner_partition_id) == HV_OK);
  assert(hv_stage2_unmap_partition(partition.partition_id) == HV_OK);
}

int main(void) {
  run_isolation_flow();
  return 0;
}
