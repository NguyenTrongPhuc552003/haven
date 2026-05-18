/**
 * Hypervisor module invariant self-tests.
 *
 * These tests verify the invariants that must hold immediately after each
 * module is initialized: zero counts, clean ownership tables, no active
 * state leaking across re-initialization. They serve as the regression
 * anchor for the "hypervisor starts in a known, consistent state" claim.
 *
 * Intended to run at the start of every CI campaign and before any
 * board validation run. A failure here indicates a broken init path
 * rather than a policy logic error.
 *
 * @file tests/selftests/test_hypervisor_invariants.c
 */

#include <assert.h>
#include <haven/budget_sched.h>
#include <haven/el2_exceptions.h>
#include <haven/iommu.h>
#include <haven/irq_ownership.h>
#include <haven/smmu.h>
#include <haven/stage2.h>
#include <haven/timer.h>
#include <stdio.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg)                                                         \
  do {                                                                         \
    printf("[FAIL] %s\n", msg);                                                \
    assert(0);                                                                 \
  } while (0)

/* -----------------------------------------------------------------------
 * Stage-2: clean after init
 * Every partition starts unmapped; unmap of any ID is denied.
 * ----------------------------------------------------------------------- */
static void selftest_stage2(void) {
  assert(hv_stage2_init() == HV_OK);

  /* No partition should be mapped. */
  assert(hv_stage2_unmap_partition(1) == HV_EPERM);
  assert(hv_stage2_unmap_partition(128) == HV_EPERM);
  assert(hv_stage2_unmap_partition(255) == HV_EPERM);

  /* Partition 0 is permanently invalid. */
  assert(hv_stage2_unmap_partition(0) == HV_EINVAL);

  TEST_PASS("selftest_stage2: no partitions mapped after init");
}

/* -----------------------------------------------------------------------
 * IRQ ownership: clean after init
 * No IRQ should be owned; is_owned_by must deny all partitions.
 * ----------------------------------------------------------------------- */
static void selftest_irq_ownership(void) {
  assert(hv_irq_owner_init() == HV_OK);

  assert(hv_irq_is_owned_by(1, 1) == HV_EPERM);
  assert(hv_irq_is_owned_by(50, 2) == HV_EPERM);
  assert(hv_irq_is_owned_by(100, 5) == HV_EPERM);

  /* IRQ ID 0 is invalid. */
  assert(hv_irq_is_owned_by(0, 1) == HV_EINVAL);

  /* Revoke of unowned IRQ is denied. */
  assert(hv_irq_revoke(1, 1) == HV_EPERM);
  assert(hv_irq_revoke(100, 5) == HV_EPERM);

  TEST_PASS("selftest_irq_ownership: no IRQs owned after init");
}

/* -----------------------------------------------------------------------
 * Budget scheduler: clean after init
 * Consuming from any partition is denied until a budget is configured.
 * ----------------------------------------------------------------------- */
static void selftest_budget_sched(void) {
  assert(hv_budget_sched_init() == HV_OK);

  /* No budget configured - consume must fail with EPERM (not EINVAL). */
  assert(hv_budget_consume(1, 1000) == HV_EPERM);
  assert(hv_budget_consume(128, 1) == HV_EPERM);

  /* Partition 0 is always invalid. */
  assert(hv_budget_consume(0, 1000) == HV_EINVAL);

  /* Period reset with timestamp 0 is invalid. */
  assert(hv_budget_reset_period(0) == HV_EINVAL);

  TEST_PASS("selftest_budget_sched: no budgets configured after init");
}

/* -----------------------------------------------------------------------
 * Timer: clean after init
 * No partition has an active deadline; check and cancel must fail.
 * ----------------------------------------------------------------------- */
static void selftest_timer(void) {
  assert(hv_timer_init() == HV_OK);

  int expired = 0;

  /* No deadline active for any partition. */
  assert(hv_timer_check_deadline(1, 1000ULL, &expired) == HV_EPERM);
  assert(hv_timer_check_deadline(128, 1000ULL, &expired) == HV_EPERM);

  /* Cancel with no active deadline is denied. */
  assert(hv_timer_cancel(1) == HV_EPERM);
  assert(hv_timer_cancel(128) == HV_EPERM);

  /* Acknowledge with no active deadline is denied. */
  assert(hv_timer_acknowledge(1) == HV_EPERM);
  assert(hv_timer_acknowledge(128) == HV_EPERM);

  /* Partition 0 is permanently invalid. */
  assert(hv_timer_set_deadline(0, 1000ULL) == HV_EINVAL);

  TEST_PASS("selftest_timer: no deadlines active after init");
}

/* -----------------------------------------------------------------------
 * IOMMU policy: clean after init
 * No group should be owned; check_group must deny all.
 * ----------------------------------------------------------------------- */
static void selftest_iommu(void) {
  assert(hv_iommu_init() == HV_OK);

  assert(hv_iommu_check_group(1, 1) == HV_EPERM);
  assert(hv_iommu_check_group(128, 5) == HV_EPERM);
  assert(hv_iommu_check_group(255, 255) == HV_EPERM);

  /* Release of unassigned group is denied. */
  assert(hv_iommu_release_group(1, 1) == HV_EPERM);
  assert(hv_iommu_release_group(128, 5) == HV_EPERM);

  /* Group 0 and partition 0 are always invalid. */
  assert(hv_iommu_check_group(0, 1) == HV_EINVAL);
  assert(hv_iommu_check_group(1, 0) == HV_EINVAL);

  TEST_PASS("selftest_iommu: no groups assigned after init");
}

/* -----------------------------------------------------------------------
 * SMMU: clean after init
 * No stream IDs allocated; check and revoke must fail.
 * ----------------------------------------------------------------------- */
static void selftest_smmu(void) {
  assert(hv_smmu_init() == HV_OK);

  hv_device_dma_t info;

  /* No stream IDs allocated - check must return EPERM. */
  assert(hv_smmu_get_device_dma(0, &info) == HV_EPERM);
  assert(hv_smmu_get_device_dma(1, &info) == HV_EPERM);

  /* Revoke of unallocated stream ID is denied. */
  assert(hv_smmu_revoke_dma_access(0) == HV_EPERM);
  assert(hv_smmu_revoke_dma_access(1) == HV_EPERM);

  /* NULL pointer rejected. */
  assert(hv_smmu_allocate_streamid(1, NULL) == HV_EINVAL);

  TEST_PASS("selftest_smmu: no stream IDs allocated after init");
}

/* -----------------------------------------------------------------------
 * EL2 exceptions: clean after init
 * All exception counters are zero; injection into disabled type is denied.
 * ----------------------------------------------------------------------- */
static void selftest_el2_exceptions(void) {
  assert(hv_el2_exceptions_init() == HV_OK);

  /* All counters start at zero. */
  assert(hv_el2_get_exception_count(HV_EXC_SYNC) == 0);
  assert(hv_el2_get_exception_count(HV_EXC_IRQ) == 0);
  assert(hv_el2_get_exception_count(HV_EXC_FIQ) == 0);
  assert(hv_el2_get_exception_count(HV_EXC_SERROR) == 0);

  /* No routes configured - unroute denied. */
  assert(hv_el2_unroute_irq(0) == HV_EPERM);
  assert(hv_el2_unroute_irq(100) == HV_EPERM);

  /* Invalid type always rejected. */
  assert(hv_el2_get_exception_count(10) == (hv_u64)-1);

  TEST_PASS("selftest_el2: clean exception state after init");
}

/* -----------------------------------------------------------------------
 * Re-initialization idempotency
 * Calling init a second time must reset state fully, not compound it.
 * ----------------------------------------------------------------------- */
static void selftest_reinit_idempotency(void) {
  /* Set up some state in each module. */
  hv_stage2_init();
  hv_irq_owner_init();
  hv_budget_sched_init();
  hv_timer_init();
  hv_iommu_init();
  hv_smmu_init();
  hv_el2_exceptions_init();

  struct hv_mem_region r = {.ipa_base = 0x10000000,
                            .pa_base = 0x10000000,
                            .size = 0x1000,
                            .attrs = 0};
  struct hv_partition_mem pm = {
      .partition_id = 1, .regions = &r, .region_count = 1};
  struct hv_irq_route route = {
      .irq_id = 10, .owner_partition_id = 1, .target_cpu = 0};
  struct hv_budget b = {
      .partition_id = 1, .period_ns = 1000000ULL, .budget_ns = 50000ULL};

  assert(hv_stage2_map_partition(&pm) == HV_OK);
  assert(hv_irq_assign(&route) == HV_OK);
  assert(hv_budget_set(&b) == HV_OK);
  assert(hv_timer_set_deadline(1, 5000ULL) == HV_OK);
  assert(hv_iommu_assign_group(1, 1) == HV_OK);

  /* Re-initialize all modules. */
  assert(hv_stage2_init() == HV_OK);
  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_budget_sched_init() == HV_OK);
  assert(hv_timer_init() == HV_OK);
  assert(hv_iommu_init() == HV_OK);
  assert(hv_smmu_init() == HV_OK);
  assert(hv_el2_exceptions_init() == HV_OK);

  /* All state must be clean again. */
  assert(hv_stage2_unmap_partition(1) == HV_EPERM);
  assert(hv_irq_is_owned_by(10, 1) == HV_EPERM);
  assert(hv_budget_consume(1, 1) == HV_EPERM);
  int exp = 0;
  assert(hv_timer_check_deadline(1, 6000ULL, &exp) == HV_EPERM);
  assert(hv_iommu_check_group(1, 1) == HV_EPERM);
  assert(hv_el2_get_exception_count(HV_EXC_SYNC) == 0);

  TEST_PASS("selftest_reinit: re-initialization fully resets all module state");
}

int main(void) {
  printf("\n=== Hypervisor Module Invariant Self-Tests ===\n\n");

  selftest_stage2();
  selftest_irq_ownership();
  selftest_budget_sched();
  selftest_timer();
  selftest_iommu();
  selftest_smmu();
  selftest_el2_exceptions();
  selftest_reinit_idempotency();

  printf("\n=== All hypervisor invariant self-tests passed ===\n\n");
  return 0;
}
