/**
 * Full happy-path integration flow - all 7 isolation modules.
 *
 * Exercises Stage-2, IRQ ownership, budget scheduling, SMMU/DMA,
 * per-partition timer, IOMMU group policy, and EL2 exception handling
 * in a single end-to-end positive flow. Represents one complete
 * partition lifecycle: set up → active → tear down.
 *
 * @file tests/integration/test_isolation_flow.c
 */

#include <assert.h>

#include "haven/budget_sched.h"
#include "haven/el2_exceptions.h"
#include "haven/iommu.h"
#include "haven/irq_ownership.h"
#include "haven/smmu.h"
#include "haven/stage2.h"
#include "haven/timer.h"

static void run_isolation_flow(void) {
  /* ------------------------------------------------------------------ */
  /* Partition layout                                                     */
  /*   Partition 1: PA 0x40000000..0x42000000 (32 MiB)                  */
  /*   IOMMU group 1 → partition 1                                       */
  /*   IRQ 48, budget 300 µs / 1 ms period                               */
  /* ------------------------------------------------------------------ */

  struct hv_mem_region region = {
      .ipa_base = 0x40000000,
      .pa_base = 0x40000000,
      .size = 0x02000000, /* 32 MiB, 4K-aligned */
      .attrs = 0,
  };

  struct hv_partition_mem partition = {
      .partition_id = 1,
      .regions = &region,
      .region_count = 1,
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

  /* ------------------------------------------------------------------ */
  /* Phase 1: Initialize all subsystems                                   */
  /* ------------------------------------------------------------------ */

  assert(hv_stage2_init() == HV_OK);
  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_budget_sched_init() == HV_OK);
  assert(hv_smmu_init() == HV_OK);
  assert(hv_timer_init() == HV_OK);
  assert(hv_iommu_init() == HV_OK);
  assert(hv_el2_exceptions_init() == HV_OK);

  /* ------------------------------------------------------------------ */
  /* Phase 2: Bring up partition 1                                        */
  /* ------------------------------------------------------------------ */

  /* Stage-2: establish memory ownership. */
  assert(hv_stage2_map_partition(&partition) == HV_OK);

  /* IOMMU: assign device group to partition. */
  assert(hv_iommu_assign_group(1, 1) == HV_OK);

  /* SMMU: allocate stream ID and configure a DMA window within P1's PA. */
  hv_u16 sid = 0;
  assert(hv_smmu_allocate_streamid(1, &sid) == HV_OK);
  assert(hv_smmu_configure_dma_window(
             sid, 0x40000000, 0x01000000, /* DMA window: 16 MiB at IPA base */
             0x40000000, /* phys_base within P1's mapped region  */
             HV_DMA_RW) == HV_OK);

  /* IRQ: assign interrupt to partition. */
  assert(hv_irq_assign(&irq) == HV_OK);

  /* Budget: configure CPU time allowance. */
  assert(hv_budget_set(&budget) == HV_OK);

  /* Timer: arm a deadline for this scheduling period. */
  assert(hv_timer_set_deadline(1, 500000ULL) == HV_OK);

  /* EL2: route IRQ 48 and inject a sync exception for partition 1. */
  assert(hv_el2_route_irq(48, 1) == HV_OK);
  assert(hv_el2_inject_exception(1, HV_EXC_SYNC, 0) == HV_OK);
  assert(hv_el2_get_exception_count(HV_EXC_SYNC) == 1);

  /* ------------------------------------------------------------------ */
  /* Phase 3: Active-partition operations                                 */
  /* ------------------------------------------------------------------ */

  /* Consume part of the budget. */
  assert(hv_budget_consume(1, 100000) == HV_OK);

  /* Verify DMA access is permitted inside the configured window. */
  assert(hv_smmu_check_dma_access(sid, 0x40000000, 0x1000, HV_DMA_RW) == HV_OK);

  /* Timer fires: expire the deadline and acknowledge it. */
  int expired = 0;
  assert(hv_timer_check_deadline(1, 600000ULL, &expired) == HV_OK);
  assert(expired == 1);
  assert(hv_timer_acknowledge(1) == HV_OK);

  /* IOMMU ownership query confirms group is still held. */
  assert(hv_iommu_check_group(1, 1) == HV_OK);

  /* Reset the scheduling period for the next cycle. */
  assert(hv_budget_reset_period(1000000) == HV_OK);

  /* ------------------------------------------------------------------ */
  /* Phase 4: Tear down partition 1                                       */
  /* ------------------------------------------------------------------ */

  assert(hv_smmu_revoke_dma_access(sid) == HV_OK);
  assert(hv_el2_unroute_irq(48) == HV_OK);
  assert(hv_irq_revoke(irq.irq_id, irq.owner_partition_id) == HV_OK);
  assert(hv_iommu_release_group(1, 1) == HV_OK);
  assert(hv_stage2_unmap_partition(partition.partition_id) == HV_OK);
}

int main(void) {
  run_isolation_flow();
  return 0;
}
