#include <assert.h>

#include "haven/budget_sched.h"
#include "haven/el2_exceptions.h"
#include "haven/irq_ownership.h"
#include "haven/smmu.h"
#include "haven/stage2.h"
#include "haven/timer.h"

static void run_negative_isolation_flow(void) {
  struct hv_mem_region zero_size_region = {
      .ipa_base = 0x60000000,
      .pa_base = 0x60000000,
      .size = 0,
      .attrs = 0,
  };
  struct hv_mem_region valid_region = {
      .ipa_base = 0x61000000,
      .pa_base = 0x61000000,
      .size = 0x1000,
      .attrs = 0,
  };
  struct hv_mem_region dma_region = {
      .ipa_base = 0x70000000,
      .pa_base = 0x70000000,
      .size = 0x10000000,
      .attrs = 0,
  };
  struct hv_mem_region misaligned_region = {
      .ipa_base = 0x62000800,
      .pa_base = 0x62000000,
      .size = 0x1000,
      .attrs = 0,
  };
  struct hv_mem_region conflicting_pa_region = {
      .ipa_base = 0x62000000,
      .pa_base = 0x61000000,
      .size = 0x1000,
      .attrs = 0,
  };
  struct hv_partition_mem zero_size_cfg = {
      .partition_id = 7,
      .regions = &zero_size_region,
      .region_count = 1,
  };
  struct hv_partition_mem valid_cfg = {
      .partition_id = 8,
      .regions = &valid_region,
      .region_count = 1,
  };
  struct hv_partition_mem dma_cfg = {
      .partition_id = 7,
      .regions = &dma_region,
      .region_count = 1,
  };
  struct hv_partition_mem misaligned_cfg = {
      .partition_id = 10,
      .regions = &misaligned_region,
      .region_count = 1,
  };
  struct hv_partition_mem conflicting_pa_cfg = {
      .partition_id = 9,
      .regions = &conflicting_pa_region,
      .region_count = 1,
  };

  struct hv_irq_route owner_route = {
      .irq_id = 64,
      .owner_partition_id = 7,
      .target_cpu = 1,
  };

  struct hv_irq_route owner_route_cpu_drift = {
      .irq_id = 64,
      .owner_partition_id = 7,
      .target_cpu = 2,
  };

  struct hv_irq_route foreign_route = {
      .irq_id = 64,
      .owner_partition_id = 9,
      .target_cpu = 2,
  };

  struct hv_irq_route invalid_cpu_route = {
      .irq_id = 65,
      .owner_partition_id = 7,
      .target_cpu = 999,
  };

  struct hv_budget invalid_budget = {
      .partition_id = 7,
      .period_ns = 100000,
      .budget_ns = 200000,
  };
  struct hv_budget tight_budget = {
      .partition_id = 7,
      .period_ns = 100000,
      .budget_ns = 20000,
  };

  hv_u16 dma_sid = 0;
  hv_device_dma_t dma_info;

  assert(hv_stage2_init() == HV_OK);
  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_budget_sched_init() == HV_OK);

  assert(hv_stage2_map_partition(NULL) == HV_EINVAL);
  assert(hv_stage2_map_partition(&zero_size_cfg) == HV_EINVAL);
  assert(hv_stage2_map_partition(&misaligned_cfg) == HV_EINVAL);
  assert(hv_stage2_map_partition(&valid_cfg) == HV_OK);
  assert(hv_stage2_map_partition(&conflicting_pa_cfg) == HV_EPERM);
  assert(hv_stage2_map_partition(&valid_cfg) == HV_EPERM);
  assert(hv_stage2_unmap_partition(0) == HV_EINVAL);
  assert(hv_stage2_unmap_partition(valid_cfg.partition_id) == HV_OK);
  assert(hv_stage2_map_partition(&conflicting_pa_cfg) == HV_OK);
  assert(hv_stage2_unmap_partition(conflicting_pa_cfg.partition_id) == HV_OK);
  assert(hv_stage2_unmap_partition(valid_cfg.partition_id) == HV_EPERM);

  assert(hv_irq_assign(&invalid_cpu_route) == HV_ENOSPC);
  assert(hv_irq_assign(&owner_route) == HV_OK);
  assert(hv_irq_assign(&owner_route_cpu_drift) == HV_EPERM);
  assert(hv_irq_assign(&foreign_route) == HV_EPERM);
  assert(hv_irq_is_owned_by(owner_route.irq_id,
                            foreign_route.owner_partition_id) == HV_EPERM);
  assert(hv_irq_revoke(owner_route.irq_id, foreign_route.owner_partition_id) ==
         HV_EPERM);
  assert(hv_irq_revoke(owner_route.irq_id, owner_route.owner_partition_id) ==
         HV_OK);

  assert(hv_budget_set(&invalid_budget) == HV_EINVAL);
  assert(hv_budget_set(&tight_budget) == HV_OK);
  assert(hv_budget_consume(0, 1000) == HV_EINVAL);
  assert(hv_budget_consume(tight_budget.partition_id, 10000) == HV_OK);
  assert(hv_budget_consume(tight_budget.partition_id, 15000) == HV_EPERM);
  assert(hv_budget_consume(3, 1000) == HV_EPERM);
  assert(hv_budget_reset_period(50000) == HV_OK);
  assert(hv_budget_reset_period(40000) == HV_EINVAL);
  assert(hv_budget_reset_period(0) == HV_EINVAL);

  assert(hv_stage2_map_partition(&dma_cfg) == HV_OK);
  assert(hv_smmu_init() == HV_OK);
  assert(hv_smmu_allocate_streamid(7, &dma_sid) == HV_OK);
  assert(hv_smmu_configure_dma_window(dma_sid, 0x70000000, 0x10000000,
                                      0x70000000,
                                      (hv_dma_access_t)0) == HV_EINVAL);
  assert(hv_smmu_configure_dma_window(dma_sid, 0x70000000, 0x10000000,
                                      0x70000000, HV_DMA_RW) == HV_OK);
  assert(hv_smmu_configure_dma_window(dma_sid, 0x71000000, 0x10000000,
                                      0x70000000, HV_DMA_RW) == HV_EPERM);
  assert(hv_smmu_configure_dma_window(dma_sid, 0x71000000, 0x10000000,
                                      0x90000000, HV_DMA_RW) == HV_EPERM);
  assert(hv_smmu_revoke_dma_access(dma_sid) == HV_OK);
  assert(hv_smmu_get_device_dma(dma_sid, &dma_info) == HV_EPERM);
  assert(hv_smmu_allocate_streamid(7, &dma_sid) == HV_OK);

  /* EL2: exception counter tracks injections. */
  assert(hv_el2_exceptions_init() == HV_OK);
  assert(hv_el2_get_exception_count(HV_EXC_SYNC) == 0);
  assert(hv_el2_inject_exception(1, HV_EXC_SYNC, 0) == HV_OK);
  assert(hv_el2_get_exception_count(HV_EXC_SYNC) == 1);

  /* EL2: injection denied when exception type is disabled. */
  assert(hv_el2_set_exception_enabled(HV_EXC_FIQ, 0) == HV_OK);
  assert(hv_el2_inject_exception(1, HV_EXC_FIQ, 0) == HV_EPERM);

  /* EL2: IRQ route exclusivity across partitions. */
  assert(hv_el2_route_irq(77, 7) == HV_OK);
  assert(hv_el2_route_irq(77, 9) == HV_EPERM);

  /* Timer: fail-closed expired-before-acknowledged guard. */
  assert(hv_timer_init() == HV_OK);
  assert(hv_timer_set_deadline(0, 1000ULL) == HV_EINVAL);
  assert(hv_timer_set_deadline(5, 0) == HV_EINVAL);
  assert(hv_timer_set_deadline(5, 5000ULL) == HV_OK);
  /* Before expiry: ack denied, cancel succeeds. */
  assert(hv_timer_acknowledge(5) == HV_EPERM);
  assert(hv_timer_cancel(5) == HV_OK);
  /* Expire then try to set new deadline without ack - must be blocked. */
  assert(hv_timer_set_deadline(5, 3000ULL) == HV_OK);
  {
    int exp = 0;
    assert(hv_timer_check_deadline(5, 6000ULL, &exp) == HV_OK);
    assert(exp == 1);
  }
  assert(hv_timer_set_deadline(5, 9000ULL) == HV_EPERM);
  assert(hv_timer_cancel(5) == HV_EPERM);
  assert(hv_timer_acknowledge(5) == HV_OK);
  assert(hv_timer_set_deadline(5, 9000ULL) == HV_OK);
}

int main(void) {
  run_negative_isolation_flow();
  return 0;
}
