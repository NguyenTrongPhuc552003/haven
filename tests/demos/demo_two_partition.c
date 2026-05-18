/**
 * Two-partition isolation demo.
 *
 * Demonstrates two concurrently active partitions operating with strict
 * isolation across all seven enforcement layers. Partition 1 models a
 * Linux-class domain (large memory, higher budget, read-write DMA).
 * Partition 2 models an RTOS-class domain (small memory, tight budget,
 * read-only DMA, higher-priority IRQ).
 *
 * Demonstrates:
 *   1. Parallel resource ownership — each partition holds its own Stage-2
 *      region, IOMMU group, SMMU stream, IRQ, budget, timer, and EL2 route
 *      simultaneously without interference.
 *   2. Cross-partition denial — any attempt by one partition to access the
 *      other's memory, IRQ, IOMMU group, or DMA window is denied.
 *   3. Independent lifecycle — tearing down partition 2 does not disturb
 *      partition 1, and vice versa.
 *
 * @file tests/demos/demo_two_partition.c
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

#define DEMO_PASS(msg) printf("[DEMO] %s\n", msg)

/* -----------------------------------------------------------------------
 * Partition 1 — Linux-class domain
 *   PA  0x40000000 .. 0x48000000  (128 MiB)
 *   IRQ 32, IOMMU group 1, budget 700 µs / 1 ms
 * ----------------------------------------------------------------------- */
static struct hv_mem_region p1_region = {
    .ipa_base = 0x40000000,
    .pa_base = 0x40000000,
    .size = 0x08000000,
    .attrs = 0,
};
static struct hv_partition_mem p1_cfg = {
    .partition_id = 1,
    .regions = &p1_region,
    .region_count = 1,
};
static struct hv_irq_route p1_irq = {
    .irq_id = 32,
    .owner_partition_id = 1,
    .target_cpu = 0,
};
static struct hv_budget p1_budget = {
    .partition_id = 1,
    .period_ns = 1000000ULL,
    .budget_ns = 700000ULL,
};

/* -----------------------------------------------------------------------
 * Partition 2 — RTOS-class domain
 *   PA  0x50000000 .. 0x50400000  (4 MiB)
 *   IRQ 33, IOMMU group 2, budget 200 µs / 1 ms
 * ----------------------------------------------------------------------- */
static struct hv_mem_region p2_region = {
    .ipa_base = 0x50000000,
    .pa_base = 0x50000000,
    .size = 0x00400000,
    .attrs = 0,
};
static struct hv_partition_mem p2_cfg = {
    .partition_id = 2,
    .regions = &p2_region,
    .region_count = 1,
};
static struct hv_irq_route p2_irq = {
    .irq_id = 33,
    .owner_partition_id = 2,
    .target_cpu = 1,
};
static struct hv_budget p2_budget = {
    .partition_id = 2,
    .period_ns = 1000000ULL,
    .budget_ns = 200000ULL,
};

/* ----------------------------------------------------------------------- */

int main(void) {
  printf("\n=== Two-Partition Isolation Demo ===\n\n");

  /* ---------------------------------------------------------------- */
  /* Phase 1: Initialize all subsystems                                */
  /* ---------------------------------------------------------------- */
  assert(hv_stage2_init() == HV_OK);
  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_budget_sched_init() == HV_OK);
  assert(hv_smmu_init() == HV_OK);
  assert(hv_timer_init() == HV_OK);
  assert(hv_iommu_init() == HV_OK);
  assert(hv_el2_exceptions_init() == HV_OK);
  DEMO_PASS("Phase 1: all subsystems initialized");

  /* ---------------------------------------------------------------- */
  /* Phase 2: Bring up both partitions simultaneously                  */
  /* ---------------------------------------------------------------- */

  /* Stage-2 memory ownership. */
  assert(hv_stage2_map_partition(&p1_cfg) == HV_OK);
  assert(hv_stage2_map_partition(&p2_cfg) == HV_OK);

  /* IOMMU group assignment. */
  assert(hv_iommu_assign_group(1, 1) == HV_OK);
  assert(hv_iommu_assign_group(2, 2) == HV_OK);

  /* SMMU: stream IDs and DMA windows. */
  hv_u16 sid1 = 0, sid2 = 0;
  assert(hv_smmu_allocate_streamid(1, &sid1) == HV_OK);
  assert(hv_smmu_allocate_streamid(2, &sid2) == HV_OK);
  assert(hv_smmu_configure_dma_window(sid1, 0x40000000, 0x04000000, 0x40000000,
                                      HV_DMA_RW) == HV_OK);
  assert(hv_smmu_configure_dma_window(sid2, 0x50000000, 0x00200000, 0x50000000,
                                      HV_DMA_RO) == HV_OK);

  /* IRQ ownership. */
  assert(hv_irq_assign(&p1_irq) == HV_OK);
  assert(hv_irq_assign(&p2_irq) == HV_OK);

  /* CPU budgets. */
  assert(hv_budget_set(&p1_budget) == HV_OK);
  assert(hv_budget_set(&p2_budget) == HV_OK);

  /* Timer deadlines. */
  assert(hv_timer_set_deadline(1, 900000ULL) == HV_OK);
  assert(hv_timer_set_deadline(2, 150000ULL) == HV_OK);

  /* EL2: route both IRQs. */
  assert(hv_el2_route_irq(32, 1) == HV_OK);
  assert(hv_el2_route_irq(33, 2) == HV_OK);

  DEMO_PASS("Phase 2: both partitions active");

  /* ---------------------------------------------------------------- */
  /* Phase 3: Independent active-phase operations                      */
  /* ---------------------------------------------------------------- */

  /* Both partitions consume budget independently. */
  assert(hv_budget_consume(1, 300000ULL) == HV_OK);
  assert(hv_budget_consume(2, 80000ULL) == HV_OK);
  DEMO_PASS("Phase 3: independent budget consumption");

  /* P2's RTOS deadline fires first; P1's is still pending. */
  int exp1 = 0, exp2 = 0;
  assert(hv_timer_check_deadline(2, 200000ULL, &exp2) == HV_OK);
  assert(exp2 == 1);
  assert(hv_timer_check_deadline(1, 200000ULL, &exp1) == HV_OK);
  assert(exp1 == 0);
  DEMO_PASS("Phase 3: P2 deadline fires before P1");

  /* P2 acknowledges and arms next deadline without affecting P1. */
  assert(hv_timer_acknowledge(2) == HV_OK);
  assert(hv_timer_set_deadline(2, 350000ULL) == HV_OK);
  assert(hv_timer_check_deadline(1, 200000ULL, &exp1) == HV_OK);
  assert(exp1 == 0);
  DEMO_PASS("Phase 3: P2 re-armed; P1 state unaffected");

  /* P2 exhausts its tight budget; P1 still has headroom. */
  assert(hv_budget_consume(2, 120000ULL) == HV_OK);
  assert(hv_budget_consume(2, 1) == HV_EPERM);
  assert(hv_budget_consume(1, 100000ULL) == HV_OK);
  DEMO_PASS("Phase 3: P2 budget exhausted; P1 continues unaffected");

  /* ---------------------------------------------------------------- */
  /* Phase 4: Cross-partition denial proofs                            */
  /* ---------------------------------------------------------------- */

  /* P2 cannot claim P1's IRQ. */
  struct hv_irq_route steal_irq = {
      .irq_id = 32, .owner_partition_id = 2, .target_cpu = 1};
  assert(hv_irq_assign(&steal_irq) == HV_EPERM);
  DEMO_PASS("Phase 4: P2 cannot steal P1 IRQ");

  /* P2 cannot claim P1's IOMMU group. */
  assert(hv_iommu_assign_group(1, 2) == HV_EPERM);
  DEMO_PASS("Phase 4: P2 cannot steal P1 IOMMU group");

  /* P2's DMA stream cannot target P1's physical memory. */
  assert(hv_smmu_revoke_dma_access(sid2) == HV_OK);
  assert(hv_smmu_allocate_streamid(2, &sid2) == HV_OK);
  assert(hv_smmu_configure_dma_window(sid2, 0x50000000, 0x00200000, 0x40000000,
                                      HV_DMA_RO) == HV_EPERM);
  DEMO_PASS("Phase 4: P2 DMA window cannot reach P1 PA");
  /* Restore a valid configuration for P2 so Phase 5 can revoke cleanly. */
  assert(hv_smmu_configure_dma_window(sid2, 0x50000000, 0x00200000, 0x50000000,
                                      HV_DMA_RO) == HV_OK);

  /* P1's EL2 IRQ route cannot be stolen by P2. */
  assert(hv_el2_route_irq(32, 2) == HV_EPERM);
  DEMO_PASS("Phase 4: P2 cannot reroute P1 EL2 IRQ");

  /* ---------------------------------------------------------------- */
  /* Phase 5: Ordered tear-down — P2 first, then P1                   */
  /* ---------------------------------------------------------------- */

  /* P2 tear-down. */
  assert(hv_timer_cancel(2) == HV_OK);
  assert(hv_smmu_revoke_dma_access(sid2) == HV_OK);
  assert(hv_el2_unroute_irq(33) == HV_OK);
  assert(hv_irq_revoke(33, 2) == HV_OK);
  assert(hv_iommu_release_group(2, 2) == HV_OK);
  assert(hv_stage2_unmap_partition(2) == HV_OK);
  DEMO_PASS("Phase 5: P2 torn down");

  /* P1 still fully operational. */
  assert(hv_budget_consume(1, 100000ULL) == HV_OK);
  assert(hv_iommu_check_group(1, 1) == HV_OK);
  assert(hv_irq_is_owned_by(32, 1) == HV_OK);
  DEMO_PASS("Phase 5: P1 intact after P2 tear-down");

  /* P1 tear-down. */
  assert(hv_timer_check_deadline(1, 1000000ULL, &exp1) == HV_OK);
  assert(exp1 == 1);
  assert(hv_timer_acknowledge(1) == HV_OK);
  assert(hv_smmu_revoke_dma_access(sid1) == HV_OK);
  assert(hv_el2_unroute_irq(32) == HV_OK);
  assert(hv_irq_revoke(32, 1) == HV_OK);
  assert(hv_iommu_release_group(1, 1) == HV_OK);
  assert(hv_stage2_unmap_partition(1) == HV_OK);
  DEMO_PASS("Phase 5: P1 torn down");

  printf("\n=== Two-partition demo completed successfully ===\n\n");
  return 0;
}
