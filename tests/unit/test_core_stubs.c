#include <assert.h>

#include "haven/budget_sched.h"
#include "haven/irq_ownership.h"
#include "haven/stage2.h"

static void test_stage2_contract(void) {
  struct hv_mem_region regions[1] = {
      {.ipa_base = 0x1000, .pa_base = 0x2000, .size = 0x1000, .attrs = 0}};
  struct hv_mem_region overlap_regions[2] = {
      {.ipa_base = 0x3000, .pa_base = 0x5000, .size = 0x2000, .attrs = 0},
      {.ipa_base = 0x3800, .pa_base = 0x8000, .size = 0x1000, .attrs = 0}};
  struct hv_partition_mem cfg = {
      .partition_id = 1, .regions = regions, .region_count = 1};
  struct hv_partition_mem overlap_cfg = {
      .partition_id = 2, .regions = overlap_regions, .region_count = 2};
  struct hv_partition_mem out_of_range_cfg = {
      .partition_id = 999, .regions = regions, .region_count = 1};

  assert(hv_stage2_init() == HV_OK);
  assert(hv_stage2_map_partition(NULL) == HV_EINVAL);
  assert(hv_stage2_map_partition(&overlap_cfg) == HV_EINVAL);
  assert(hv_stage2_map_partition(&out_of_range_cfg) == HV_ENOSPC);
  assert(hv_stage2_map_partition(&cfg) == HV_OK);
  assert(hv_stage2_map_partition(&cfg) == HV_EPERM);
  assert(hv_stage2_unmap_partition(0) == HV_EINVAL);
  assert(hv_stage2_unmap_partition(999) == HV_ENOSPC);
  assert(hv_stage2_unmap_partition(1) == HV_OK);
  assert(hv_stage2_unmap_partition(1) == HV_EPERM);
}

static void test_irq_contract(void) {
  struct hv_irq_route route = {
      .irq_id = 32, .owner_partition_id = 1, .target_cpu = 0};
  struct hv_irq_route foreign_route = {
      .irq_id = 32, .owner_partition_id = 2, .target_cpu = 0};

  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_irq_assign(NULL) == HV_EINVAL);
  assert(hv_irq_assign(&route) == HV_OK);
  assert(hv_irq_assign(&foreign_route) == HV_EPERM);
  assert(hv_irq_revoke(0, 1) == HV_EINVAL);
  assert(hv_irq_revoke(32, 2) == HV_EPERM);
  assert(hv_irq_is_owned_by(32, 2) == HV_EPERM);
  assert(hv_irq_is_owned_by(2048, 1) == HV_ENOSPC);
  assert(hv_irq_revoke(32, 1) == HV_OK);
  assert(hv_irq_is_owned_by(0, 1) == HV_EINVAL);
  assert(hv_irq_is_owned_by(32, 1) == HV_EPERM);
}

static void test_budget_contract(void) {
  struct hv_budget good = {
      .partition_id = 1, .period_ns = 1000000, .budget_ns = 500000};
  struct hv_budget bad = {
      .partition_id = 1, .period_ns = 100000, .budget_ns = 200000};
  struct hv_budget unconfigured = {
      .partition_id = 2, .period_ns = 1000000, .budget_ns = 100000};

  assert(hv_budget_sched_init() == HV_OK);
  assert(hv_budget_set(NULL) == HV_EINVAL);
  assert(hv_budget_set(&bad) == HV_EINVAL);
  assert(hv_budget_set(&good) == HV_OK);
  assert(hv_budget_consume(0, 1000) == HV_EINVAL);
  assert(hv_budget_consume(1, 0) == HV_EINVAL);
  assert(hv_budget_consume(1, 1000) == HV_OK);
  assert(hv_budget_consume(2, 1000) == HV_EPERM);
  assert(hv_budget_consume(1, 600000) == HV_EPERM);
  assert(hv_budget_set(&unconfigured) == HV_OK);
  assert(hv_budget_consume(257, 1000) == HV_ENOSPC);
  assert(hv_budget_reset_period(0) == HV_EINVAL);
  assert(hv_budget_reset_period(1000000) == HV_OK);
  assert(hv_budget_consume(1, 200000) == HV_OK);
}

int main(void) {
  test_stage2_contract();
  test_irq_contract();
  test_budget_contract();
  return 0;
}
