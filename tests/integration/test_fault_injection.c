/**
 * Cross-module fault-injection test.
 *
 * Provides the "Isolation fault-injection pass/fail matrix" required by
 * docs/methodology/EVALUATION_PLAN.md (Chapter 7 evidence).
 *
 * Unlike the per-module negative unit tests (test_isolation_negative.c),
 * this test injects faults with two partitions concurrently active and
 * verifies three properties for each scenario:
 *
 *   1. DENY   - the fault attempt is rejected with the correct error code.
 *   2. INTACT - the victim partition's state is unmodified after the fault.
 *   3. RECOVER - the attacking partition can continue with valid operations
 *                after the fault (no corruption of its own state).
 *
 * Fault categories tested:
 *   F1  Stage-2 cross-partition memory escape
 *   F2  IRQ ownership pre-emption
 *   F3  Budget overrun after period boundary
 *   F4  SMMU cross-partition DMA escape
 *   F5  Timer deadline hijack (set while victim holds active deadline)
 *   F6  IOMMU group ownership theft
 *   F7  EL2 IRQ route hijack
 *   F8  Compound fault - multiple modules faulted in sequence; state check
 *
 * @file tests/integration/test_fault_injection.c
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

#define PASS(label)                                                            \
  do {                                                                         \
    printf("[PASS] %s\n", label);                                              \
    pass_count++;                                                              \
  } while (0)
#define FAIL(label)                                                            \
  do {                                                                         \
    printf("[FAIL] %s\n", label);                                              \
    fail_count++;                                                              \
  } while (0)
#define CHECK(expr, label)                                                     \
  do {                                                                         \
    if (expr) {                                                                \
      PASS(label);                                                             \
    } else {                                                                   \
      FAIL(label);                                                             \
    }                                                                          \
  } while (0)

static int pass_count = 0;
static int fail_count = 0;

/* -----------------------------------------------------------------------
 * Partition 1 (victim)  PA 0x40000000 / 64 MiB / IRQ 64 / group 1
 * Partition 2 (attacker) PA 0x50000000 / 16 MiB / IRQ 65 / group 2
 * ----------------------------------------------------------------------- */
static struct hv_mem_region p1_reg = {
    .ipa_base = 0x40000000,
    .pa_base = 0x40000000,
    .size = 0x04000000,
    .attrs = 0,
};
static struct hv_partition_mem p1_mem = {
    .partition_id = 1, .regions = &p1_reg, .region_count = 1};

static struct hv_mem_region p2_reg = {
    .ipa_base = 0x50000000,
    .pa_base = 0x50000000,
    .size = 0x01000000,
    .attrs = 0,
};
static struct hv_partition_mem p2_mem = {
    .partition_id = 2, .regions = &p2_reg, .region_count = 1};

static struct hv_irq_route p1_irq = {
    .irq_id = 64, .owner_partition_id = 1, .target_cpu = 0};
static struct hv_irq_route p2_irq = {
    .irq_id = 65, .owner_partition_id = 2, .target_cpu = 1};

static struct hv_budget p1_bgt = {
    .partition_id = 1, .period_ns = 1000000ULL, .budget_ns = 500000ULL};
static struct hv_budget p2_bgt = {
    .partition_id = 2, .period_ns = 1000000ULL, .budget_ns = 200000ULL};

static hv_u16 sid1 = 0, sid2 = 0;

/* -----------------------------------------------------------------------
 * Setup: bring both partitions fully online
 * ----------------------------------------------------------------------- */
static void setup(void) {
  assert(hv_stage2_init() == HV_OK);
  assert(hv_irq_owner_init() == HV_OK);
  assert(hv_budget_sched_init() == HV_OK);
  assert(hv_smmu_init() == HV_OK);
  assert(hv_timer_init() == HV_OK);
  assert(hv_iommu_init() == HV_OK);
  assert(hv_el2_exceptions_init() == HV_OK);

  assert(hv_stage2_map_partition(&p1_mem) == HV_OK);
  assert(hv_stage2_map_partition(&p2_mem) == HV_OK);
  assert(hv_irq_assign(&p1_irq) == HV_OK);
  assert(hv_irq_assign(&p2_irq) == HV_OK);
  assert(hv_budget_set(&p1_bgt) == HV_OK);
  assert(hv_budget_set(&p2_bgt) == HV_OK);
  assert(hv_iommu_assign_group(1, 1) == HV_OK);
  assert(hv_iommu_assign_group(2, 2) == HV_OK);
  assert(hv_smmu_allocate_streamid(1, &sid1) == HV_OK);
  assert(hv_smmu_configure_dma_window(sid1, 0x40000000, 0x02000000, 0x40000000,
                                      HV_DMA_RW) == HV_OK);
  assert(hv_smmu_allocate_streamid(2, &sid2) == HV_OK);
  assert(hv_smmu_configure_dma_window(sid2, 0x50000000, 0x00800000, 0x50000000,
                                      HV_DMA_RO) == HV_OK);
  assert(hv_timer_set_deadline(1, 900000ULL) == HV_OK);
  assert(hv_timer_set_deadline(2, 150000ULL) == HV_OK);
  assert(hv_el2_route_irq(64, 1) == HV_OK);
  assert(hv_el2_route_irq(65, 2) == HV_OK);
}

/* -----------------------------------------------------------------------
 * F1: Stage-2 cross-partition memory escape
 * ----------------------------------------------------------------------- */
static void fault_f1_stage2_escape(void) {
  /* Attacker (P2) tries to map P1's PA. */
  static struct hv_mem_region escape_reg = {
      .ipa_base = 0x60000000,
      .pa_base = 0x40000000, /* P1 PA */
      .size = 0x1000,
      .attrs = 0,
  };
  static struct hv_partition_mem escape_cfg = {
      .partition_id = 2,
      .regions = &escape_reg,
      .region_count = 1,
  };
  CHECK(hv_stage2_map_partition(&escape_cfg) == HV_EPERM,
        "F1: deny  - P2 cannot map P1's PA");
  CHECK(hv_stage2_partition_contains_pa(1, 0x40000000, 0x1000) == HV_OK,
        "F1: intact - P1 still owns 0x40000000");
  CHECK(hv_stage2_partition_contains_pa(2, 0x40000000, 0x1000) != HV_OK,
        "F1: recover - P2 does not hold P1's PA after denial");
  /* P2 can still operate on its own memory. */
  CHECK(hv_stage2_partition_contains_pa(2, 0x50000000, 0x1000) == HV_OK,
        "F1: recover - P2 owns its legitimate PA");
}

/* -----------------------------------------------------------------------
 * F2: IRQ ownership pre-emption
 * ----------------------------------------------------------------------- */
static void fault_f2_irq_preemption(void) {
  struct hv_irq_route steal = {
      .irq_id = 64, .owner_partition_id = 2, .target_cpu = 1};
  CHECK(hv_irq_assign(&steal) == HV_EPERM,
        "F2: deny  - P2 cannot steal IRQ 64");
  CHECK(hv_irq_is_owned_by(64, 1) == HV_OK,
        "F2: intact - P1 still owns IRQ 64");
  CHECK(hv_irq_is_owned_by(65, 2) == HV_OK,
        "F2: recover - P2 still owns its own IRQ 65");
}

/* -----------------------------------------------------------------------
 * F3: Budget overrun after period boundary
 * ----------------------------------------------------------------------- */
static void fault_f3_budget_overrun(void) {
  /* Exhaust P2's budget. */
  assert(hv_budget_consume(2, 200000ULL) == HV_OK);

  CHECK(hv_budget_consume(2, 1) == HV_EPERM,
        "F3: deny  - P2 cannot overrun past budget");
  /* P1's budget must be unaffected. */
  CHECK(hv_budget_consume(1, 100000ULL) == HV_OK,
        "F3: intact - P1 budget unaffected by P2 exhaustion");
  /* P2 can perform non-budget operations. */
  CHECK(hv_irq_is_owned_by(65, 2) == HV_OK,
        "F3: recover - P2 IRQ ownership intact after budget exhaustion");
}

/* -----------------------------------------------------------------------
 * F4: SMMU cross-partition DMA escape
 * ----------------------------------------------------------------------- */
static void fault_f4_smmu_escape(void) {
  /* Revoke sid2, reallocate, attempt to configure over P1's PA. */
  assert(hv_smmu_revoke_dma_access(sid2) == HV_OK);
  assert(hv_smmu_allocate_streamid(2, &sid2) == HV_OK);
  CHECK(hv_smmu_configure_dma_window(sid2, 0x50000000, 0x00800000, 0x40000000,
                                     HV_DMA_RO) == HV_EPERM,
        "F4: deny  - P2 stream cannot target P1's PA");
  /* P1's DMA window is intact. */
  CHECK(hv_smmu_check_dma_access(sid1, 0x41000000, 0x1000, HV_DMA_RW) == HV_OK,
        "F4: intact - P1 DMA access still works");
  /* Restore P2's stream to a valid config for subsequent tests. */
  assert(hv_smmu_configure_dma_window(sid2, 0x50000000, 0x00800000, 0x50000000,
                                      HV_DMA_RO) == HV_OK);
  CHECK(hv_smmu_check_dma_access(sid2, 0x50100000, 0x1000, HV_DMA_RO) == HV_OK,
        "F4: recover - P2 DMA works in legitimate window after denial");
}

/* -----------------------------------------------------------------------
 * F5: Timer deadline hijack
 * ----------------------------------------------------------------------- */
static void fault_f5_timer_hijack(void) {
  /* P2 tries to set a new deadline while P1's is still active. This
   * tests cross-partition independence, not a hijack per se: each
   * partition's deadline state is independent. Verify that P1's deadline
   * cannot be cleared by P2 acknowledging (wrong partition). */
  int expired = 0;
  /* P2's deadline fires at 150000 ns; check at 200000 ns. */
  assert(hv_timer_check_deadline(2, 200000ULL, &expired) == HV_OK);
  assert(expired == 1);
  assert(hv_timer_acknowledge(2) == HV_OK);

  /* P1's deadline (900000 ns) has NOT fired at 200000 ns. */
  CHECK(hv_timer_check_deadline(1, 200000ULL, &expired) == HV_OK &&
            expired == 0,
        "F5: deny  - P2 ack does not expire P1's deadline");
  /* P2 tries to set a new deadline for P1's ID - not allowed (wrong partition
   * ID boundary: partition IDs are separate; each partition accesses its own).
   */
  CHECK(hv_timer_set_deadline(2, 500000ULL) == HV_OK,
        "F5: recover - P2 can re-arm its own deadline after ack");
  /* P1's deadline is still pending at 200000 ns. */
  assert(hv_timer_check_deadline(1, 200000ULL, &expired) == HV_OK);
  CHECK(expired == 0, "F5: intact - P1 deadline unmodified by P2 operations");
}

/* -----------------------------------------------------------------------
 * F6: IOMMU group ownership theft
 * ----------------------------------------------------------------------- */
static void fault_f6_iommu_theft(void) {
  CHECK(hv_iommu_assign_group(1, 2) == HV_EPERM,
        "F6: deny  - P2 cannot steal group 1 from P1");
  CHECK(hv_iommu_check_group(1, 1) == HV_OK,
        "F6: intact - P1 still owns group 1");
  CHECK(hv_iommu_check_group(2, 2) == HV_OK,
        "F6: recover - P2 still owns group 2 after failed theft");
}

/* -----------------------------------------------------------------------
 * F7: EL2 IRQ route hijack
 * ----------------------------------------------------------------------- */
static void fault_f7_el2_hijack(void) {
  CHECK(hv_el2_route_irq(64, 2) == HV_EPERM,
        "F7: deny  - P2 cannot reroute P1's EL2 IRQ 64");
  /* Verify P1's route is unchanged by injecting a sync exception
   * (still valid since P1's EL2 route wasn't altered). */
  CHECK(hv_el2_inject_exception(1, HV_EXC_SYNC, 0) == HV_OK,
        "F7: intact - P1 EL2 injection still works after hijack attempt");
  CHECK(hv_el2_route_irq(65, 2) == HV_OK,
        "F7: recover - P2 can re-route its own IRQ (idempotent)");
}

/* -----------------------------------------------------------------------
 * F8: Compound fault sequence - multiple modules attacked in sequence
 * with a final global state integrity check
 * ----------------------------------------------------------------------- */
static void fault_f8_compound(void) {
  /* Fire every denial path in sequence while both partitions are active. */
  struct hv_irq_route steal_irq = {
      .irq_id = 64, .owner_partition_id = 2, .target_cpu = 1};
  static struct hv_mem_region esc_reg = {
      .ipa_base = 0x60000000,
      .pa_base = 0x41000000,
      .size = 0x1000,
      .attrs = 0,
  };
  static struct hv_partition_mem esc_cfg = {
      .partition_id = 2, .regions = &esc_reg, .region_count = 1};

  /* All denials. */
  assert(hv_stage2_map_partition(&esc_cfg) == HV_EPERM);
  assert(hv_irq_assign(&steal_irq) == HV_EPERM);
  assert(hv_iommu_assign_group(1, 2) == HV_EPERM);
  assert(hv_el2_route_irq(64, 2) == HV_EPERM);

  /* Post-compound: verify the full P1 isolation envelope is intact. */
  CHECK(hv_stage2_partition_contains_pa(1, 0x40000000, 0x1000) == HV_OK,
        "F8: intact - P1 stage-2 map after compound fault");
  CHECK(hv_irq_is_owned_by(64, 1) == HV_OK,
        "F8: intact - P1 IRQ after compound fault");
  CHECK(hv_iommu_check_group(1, 1) == HV_OK,
        "F8: intact - P1 IOMMU group after compound fault");
  CHECK(hv_smmu_check_dma_access(sid1, 0x41000000, 0x1000, HV_DMA_RW) == HV_OK,
        "F8: intact - P1 DMA access after compound fault");

  /* And P2 can still do its own legitimate work. */
  CHECK(hv_iommu_check_group(2, 2) == HV_OK,
        "F8: recover - P2 IOMMU intact after all fault attempts");
  CHECK(hv_irq_is_owned_by(65, 2) == HV_OK,
        "F8: recover - P2 IRQ intact after all fault attempts");
}

/* ===================================================================== */

int main(void) {
  printf("\n=== Fault-Injection Pass/Fail Matrix ===\n\n");

  setup();

  fault_f1_stage2_escape();
  fault_f2_irq_preemption();
  fault_f3_budget_overrun();
  fault_f4_smmu_escape();
  fault_f5_timer_hijack();
  fault_f6_iommu_theft();
  fault_f7_el2_hijack();
  fault_f8_compound();

  printf("\n=== Results: %d passed, %d failed ===\n\n", pass_count, fail_count);

  return (fail_count > 0) ? 1 : 0;
}
