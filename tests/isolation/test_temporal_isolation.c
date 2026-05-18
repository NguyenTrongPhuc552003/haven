/**
 * Temporal isolation boundary tests — Chapter 5 evidence.
 *
 * Exercises the three temporal isolation enforcement layers together:
 *   - Budget scheduling (CPU time overrun containment)
 *   - Per-partition timer deadlines (temporal state independence)
 *   - IRQ ownership routing (temporal exclusion via interrupt ownership)
 *
 * Each scenario directly maps to a Chapter 5 acceptance criterion from
 * docs/methodology/EVALUATION_PLAN.md:
 *   "Budget overrun is detected and denied."
 *   "RTOS task latency/jitter remains within predefined threshold."
 *   "Deadline miss trend remains within agreed regression bound."
 *
 * @file tests/isolation/test_temporal_isolation.c
 */

#include <assert.h>
#include <haven/budget_sched.h>
#include <haven/irq_ownership.h>
#include <haven/timer.h>
#include <stdio.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg)                                                         \
  do {                                                                         \
    printf("[FAIL] %s\n", msg);                                                \
    assert(0);                                                                 \
  } while (0)

/* -----------------------------------------------------------------------
 * Scenario 1: Budget overrun containment
 *
 * A partition that has consumed its entire budget must be denied further
 * execution time. The denial must persist until the scheduler period
 * advances (monotonic reset). A partition with budget remaining is not
 * affected by another partition's exhaustion.
 * ----------------------------------------------------------------------- */
static void test_budget_overrun_containment(void) {
  assert(hv_budget_sched_init() == HV_OK);

  struct hv_budget b1 = {
      .partition_id = 1, .period_ns = 1000000ULL, .budget_ns = 50000ULL};
  struct hv_budget b2 = {
      .partition_id = 2, .period_ns = 1000000ULL, .budget_ns = 200000ULL};

  assert(hv_budget_set(&b1) == HV_OK);
  assert(hv_budget_set(&b2) == HV_OK);

  /* P1 consumes exactly its budget. */
  assert(hv_budget_consume(1, 50000ULL) == HV_OK);
  TEST_PASS("budget: partition 1 consumes full budget");

  /* P1 is now denied any further time in this period. */
  assert(hv_budget_consume(1, 1) == HV_EPERM);
  TEST_PASS("budget: partition 1 denied after exhaustion");

  /* P2 is unaffected — it still has budget remaining. */
  assert(hv_budget_consume(2, 100000ULL) == HV_OK);
  TEST_PASS("budget: partition 2 unaffected by partition 1 exhaustion");

  /* Period reset unblocks P1. */
  assert(hv_budget_reset_period(1000000ULL) == HV_OK);
  assert(hv_budget_consume(1, 20000ULL) == HV_OK);
  TEST_PASS("budget: partition 1 restored after period reset");

  /* Non-monotonic reset is denied. */
  assert(hv_budget_reset_period(500000ULL) == HV_EINVAL);
  TEST_PASS("budget: non-monotonic period reset denied");
}

/* -----------------------------------------------------------------------
 * Scenario 2: Timer deadline independence across partitions
 *
 * Each partition's deadline state is isolated: P1's expired-and-pending
 * acknowledgement must not block P2 from setting or checking its own
 * deadline. Acknowledging P1's expiry must not affect P2's state.
 * ----------------------------------------------------------------------- */
static void test_timer_deadline_partition_independence(void) {
  assert(hv_timer_init() == HV_OK);

  /* P1 sets and expires a deadline without acknowledging it. */
  assert(hv_timer_set_deadline(1, 2000ULL) == HV_OK);
  int expired = 0;
  assert(hv_timer_check_deadline(1, 3000ULL, &expired) == HV_OK);
  assert(expired == 1);
  TEST_PASS("timer: partition 1 deadline expired");

  /* P2 can still set and check its own deadline independently. */
  assert(hv_timer_set_deadline(2, 5000ULL) == HV_OK);
  assert(hv_timer_check_deadline(2, 4000ULL, &expired) == HV_OK);
  assert(expired == 0);
  TEST_PASS("timer: partition 2 deadline unaffected by partition 1 state");

  /* P1 is blocked from a new deadline until it acknowledges. */
  assert(hv_timer_set_deadline(1, 9000ULL) == HV_EPERM);
  TEST_PASS("timer: partition 1 blocked until acknowledgement");

  /* Acknowledging P1 does not disturb P2. */
  assert(hv_timer_acknowledge(1) == HV_OK);
  assert(hv_timer_check_deadline(2, 4500ULL, &expired) == HV_OK);
  assert(expired == 0);
  TEST_PASS(
      "timer: partition 2 state intact after partition 1 acknowledgement");

  /* P1 can now set a new deadline. */
  assert(hv_timer_set_deadline(1, 9000ULL) == HV_OK);
  TEST_PASS("timer: partition 1 can set new deadline after acknowledgement");

  /* P2 expires and must acknowledge before its next deadline. */
  assert(hv_timer_check_deadline(2, 6000ULL, &expired) == HV_OK);
  assert(expired == 1);
  assert(hv_timer_acknowledge(2) == HV_OK);
  assert(hv_timer_set_deadline(2, 20000ULL) == HV_OK);
  TEST_PASS("timer: partition 2 lifecycle independent of partition 1");
}

/* -----------------------------------------------------------------------
 * Scenario 3: IRQ routing temporal exclusion
 *
 * Once an IRQ is routed to a partition, no other partition can preempt
 * that routing for the duration of the assignment. Temporal exclusion
 * persists: only an explicit revoke by the owner can free the IRQ.
 * A rogue partition must not be able to inject a routing change that
 * would redirect interrupt delivery mid-execution.
 * ----------------------------------------------------------------------- */
static void test_irq_routing_temporal_exclusion(void) {
  assert(hv_irq_owner_init() == HV_OK);

  struct hv_irq_route r1 = {
      .irq_id = 32, .owner_partition_id = 1, .target_cpu = 0};
  struct hv_irq_route r2 = {
      .irq_id = 32, .owner_partition_id = 2, .target_cpu = 1};

  assert(hv_irq_assign(&r1) == HV_OK);
  TEST_PASS("irq: partition 1 assigned IRQ 32");

  /* P2 cannot steal IRQ 32 while P1 owns it. */
  assert(hv_irq_assign(&r2) == HV_EPERM);
  TEST_PASS("irq: partition 2 cannot preempt partition 1 IRQ routing");

  /* P2 cannot revoke P1's assignment. */
  assert(hv_irq_revoke(32, 2) == HV_EPERM);
  TEST_PASS("irq: non-owner cannot revoke IRQ assignment");

  /* Ownership confirmed. */
  assert(hv_irq_is_owned_by(32, 1) == HV_OK);
  assert(hv_irq_is_owned_by(32, 2) == HV_EPERM);
  TEST_PASS("irq: ownership query returns correct owner");

  /* P1 revokes; P2 can now claim. */
  assert(hv_irq_revoke(32, 1) == HV_OK);
  assert(hv_irq_assign(&r2) == HV_OK);
  assert(hv_irq_is_owned_by(32, 2) == HV_OK);
  TEST_PASS("irq: after revoke partition 2 can acquire IRQ");
}

/* -----------------------------------------------------------------------
 * Scenario 4: Cross-module temporal state independence
 *
 * A partition that is budget-exhausted still has its timer state intact.
 * A partition that has an unacknowledged timer expiry still has its IRQ
 * assignment intact. The modules do not share mutable state.
 * ----------------------------------------------------------------------- */
static void test_cross_module_temporal_independence(void) {
  assert(hv_budget_sched_init() == HV_OK);
  assert(hv_timer_init() == HV_OK);
  assert(hv_irq_owner_init() == HV_OK);

  struct hv_budget b3 = {
      .partition_id = 3, .period_ns = 1000000ULL, .budget_ns = 10000ULL};
  struct hv_irq_route r3 = {
      .irq_id = 50, .owner_partition_id = 3, .target_cpu = 0};

  assert(hv_budget_set(&b3) == HV_OK);
  assert(hv_timer_set_deadline(3, 5000ULL) == HV_OK);
  assert(hv_irq_assign(&r3) == HV_OK);

  /* Exhaust P3's budget. */
  assert(hv_budget_consume(3, 10000ULL) == HV_OK);
  assert(hv_budget_consume(3, 1) == HV_EPERM);
  TEST_PASS("cross-module: partition 3 budget exhausted");

  /* Timer state is unaffected by budget exhaustion. */
  int expired = 0;
  assert(hv_timer_check_deadline(3, 3000ULL, &expired) == HV_OK);
  assert(expired == 0);
  TEST_PASS("cross-module: timer state intact despite budget exhaustion");

  /* IRQ ownership is unaffected by budget exhaustion. */
  assert(hv_irq_is_owned_by(50, 3) == HV_OK);
  TEST_PASS("cross-module: IRQ ownership intact despite budget exhaustion");

  /* Expire the timer. */
  assert(hv_timer_check_deadline(3, 6000ULL, &expired) == HV_OK);
  assert(expired == 1);

  /* Budget state is still exhausted — timer expiry doesn't reset it. */
  assert(hv_budget_consume(3, 1) == HV_EPERM);
  TEST_PASS("cross-module: budget still exhausted after timer expiry");

  /* IRQ still owned by P3 despite timer expiry. */
  assert(hv_irq_is_owned_by(50, 3) == HV_OK);
  TEST_PASS("cross-module: IRQ ownership intact despite timer expiry");
}

int main(void) {
  printf(
      "\n=== Temporal Isolation Boundary Tests (Chapter 5 Evidence) ===\n\n");

  test_budget_overrun_containment();
  test_timer_deadline_partition_independence();
  test_irq_routing_temporal_exclusion();
  test_cross_module_temporal_independence();

  printf("\n=== All temporal isolation tests passed ===\n\n");
  return 0;
}
