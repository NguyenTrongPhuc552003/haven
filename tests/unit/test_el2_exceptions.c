/**
 * Unit tests for EL2 exception handling.
 *
 * Tests cover:
 *   - Handler registration (global and partition-specific)
 *   - IRQ routing and unrouting
 *   - Exception enable/disable
 *   - Exception statistics counting
 *   - Exception injection
 *
 * @file tests/unit/test_el2_exceptions.c
 */

#include <assert.h>
#include <haven/el2_exceptions.h>
#include <stdio.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg)                      \
	do {                                \
		printf("[FAIL] %s\n", msg); \
		assert(0);                  \
	} while (0)

/**
 * Test handler that records calls.
 */
static int test_handler_called = 0;

static hv_status_t test_handler(hv_exc_context_t *ctx)
{
	(void)ctx; /* Mark unused parameter */
	test_handler_called++;
	return HV_OK;
}

/**
 * Test: Initialize EL2 exception handling.
 */
static void test_el2_init(void)
{
	hv_status_t status = hv_el2_exceptions_init();
	assert(status == HV_OK);
	TEST_PASS("el2_init: basic initialization");
}

/**
 * Test: Register global exception handlers.
 */
static void test_el2_register_handler(void)
{
	hv_el2_exceptions_init();

	hv_status_t status;

	/* Register SYNC handler. */
	status = hv_el2_register_handler(HV_EXC_SYNC, test_handler);
	assert(status == HV_OK);
	TEST_PASS("el2_register_handler: register sync handler");

	/* Register IRQ handler. */
	status = hv_el2_register_handler(HV_EXC_IRQ, test_handler);
	assert(status == HV_OK);
	TEST_PASS("el2_register_handler: register irq handler");

	/* Register NULL handler (clear handler). */
	status = hv_el2_register_handler(HV_EXC_SYNC, NULL);
	assert(status == HV_OK);
	TEST_PASS("el2_register_handler: clear handler with NULL");

	/* Negative: Invalid exception type. */
	status = hv_el2_register_handler(10, test_handler);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_register_handler: rejects invalid exception type");
}

/**
 * Test: Register partition-specific handlers.
 */
static void test_el2_register_partition_handler(void)
{
	hv_el2_exceptions_init();

	hv_status_t status;

	/* Register partition 0, SYNC exception handler. */
	status =
		hv_el2_register_partition_handler(0, HV_EXC_SYNC, test_handler);
	assert(status == HV_OK);
	TEST_PASS("el2_register_partition_handler: register partition 0 sync");

	/* Register partition 1, IRQ exception handler. */
	status = hv_el2_register_partition_handler(1, HV_EXC_IRQ, test_handler);
	assert(status == HV_OK);
	TEST_PASS("el2_register_partition_handler: register partition 1 irq");

	/* Update existing handler. */
	status = hv_el2_register_partition_handler(0, HV_EXC_SYNC, NULL);
	assert(status == HV_OK);
	TEST_PASS("el2_register_partition_handler: clear partition handler");

	/* Negative: Invalid partition. */
	status = hv_el2_register_partition_handler(256, HV_EXC_SYNC,
						   test_handler);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_register_partition_handler: rejects invalid partition");

	/* Negative: Invalid exception type. */
	status = hv_el2_register_partition_handler(0, 10, test_handler);
	assert(status == HV_EINVAL);
	TEST_PASS(
		"el2_register_partition_handler: rejects invalid exception type");
}

/**
 * Test: Route IRQs to partitions.
 */
static void test_el2_route_irq(void)
{
	hv_el2_exceptions_init();

	hv_status_t status;

	/* Route IRQ 10 to partition 0. */
	status = hv_el2_route_irq(10, 0);
	assert(status == HV_OK);
	TEST_PASS("el2_route_irq: route irq 10 to partition 0");

	/* Route same IRQ to same partition (allowed). */
	status = hv_el2_route_irq(10, 0);
	assert(status == HV_OK);
	TEST_PASS("el2_route_irq: re-route same irq to same partition allowed");

	/* Negative: Route same IRQ to different partition. */
	status = hv_el2_route_irq(10, 1);
	assert(status == HV_EPERM);
	TEST_PASS("el2_route_irq: denies re-route to different partition");

	/* Route different IRQ to same partition. */
	status = hv_el2_route_irq(11, 0);
	assert(status == HV_OK);
	TEST_PASS("el2_route_irq: route different irq allowed");

	/* Negative: Invalid IRQ number. */
	status = hv_el2_route_irq(256, 0);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_route_irq: rejects invalid irq");

	/* Negative: Invalid partition. */
	status = hv_el2_route_irq(20, 256);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_route_irq: rejects invalid partition");
}

/**
 * Test: Unroute IRQs from partitions.
 */
static void test_el2_unroute_irq(void)
{
	hv_el2_exceptions_init();

	hv_status_t status;

	/* Route then unroute. */
	status = hv_el2_route_irq(10, 0);
	assert(status == HV_OK);

	status = hv_el2_unroute_irq(10);
	assert(status == HV_OK);
	TEST_PASS("el2_unroute_irq: unroute succeeds after route");

	/* Negative: Unroute already unrouted IRQ. */
	status = hv_el2_unroute_irq(10);
	assert(status == HV_EPERM);
	TEST_PASS("el2_unroute_irq: denies double unroute");

	/* Negative: Invalid IRQ. */
	status = hv_el2_unroute_irq(256);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_unroute_irq: rejects invalid irq");
}

/**
 * Test: Get exception context.
 */
static void test_el2_get_context(void)
{
	hv_el2_exceptions_init();

	hv_exc_context_t *ctx = hv_el2_get_context();
	assert(ctx != NULL);
	TEST_PASS("el2_get_context: returns valid context pointer");

	/* Verify context structure accessible. */
	ctx->pc = 0x1000;
	ctx->source_partition = 42;
	assert(ctx->pc == 0x1000);
	assert(ctx->source_partition == 42);
	TEST_PASS("el2_get_context: context fields accessible");
}

/**
 * Test: Enable/disable exceptions.
 */
static void test_el2_set_exception_enabled(void)
{
	hv_el2_exceptions_init();

	hv_status_t status;

	/* Disable SYNC exceptions. */
	status = hv_el2_set_exception_enabled(HV_EXC_SYNC, 0);
	assert(status == HV_OK);
	TEST_PASS("el2_set_exception_enabled: disable sync");

	/* Enable IRQ exceptions. */
	status = hv_el2_set_exception_enabled(HV_EXC_IRQ, 1);
	assert(status == HV_OK);
	TEST_PASS("el2_set_exception_enabled: enable irq");

	/* Negative: Invalid exception type. */
	status = hv_el2_set_exception_enabled(10, 1);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_set_exception_enabled: rejects invalid exception type");
}

/**
 * Test: Get exception statistics.
 */
static void test_el2_get_exception_count(void)
{
	hv_el2_exceptions_init();

	hv_u64 count;

	/* Get SYNC exception count (should be 0 initially). */
	count = hv_el2_get_exception_count(HV_EXC_SYNC);
	assert(count == 0);
	TEST_PASS("el2_get_exception_count: initial count is 0");

	/* Inject and verify count increments. */
	hv_el2_inject_exception(1, HV_EXC_SYNC, 0);
	count = hv_el2_get_exception_count(HV_EXC_SYNC);
	assert(count == 1);
	TEST_PASS("el2_get_exception_count: increments after inject");

	/* Inject again and verify counter accumulates. */
	hv_el2_inject_exception(1, HV_EXC_SYNC, 0);
	count = hv_el2_get_exception_count(HV_EXC_SYNC);
	assert(count == 2);
	TEST_PASS(
		"el2_get_exception_count: accumulates across multiple injects");

	/* Negative: Invalid exception type. */
	count = hv_el2_get_exception_count(10);
	assert(count == (hv_u64)-1);
	TEST_PASS("el2_get_exception_count: returns -1 for invalid type");
}

/**
 * Test: Inject exception into partition.
 */
static void test_el2_inject_exception(void)
{
	hv_el2_exceptions_init();

	hv_status_t status;

	/* Inject SYNC exception to partition 0, vector 0. */
	status = hv_el2_inject_exception(0, HV_EXC_SYNC, 0);
	assert(status == HV_OK);
	TEST_PASS("el2_inject_exception: inject sync to partition 0");

	/* Inject IRQ exception to partition 1, vector 1. */
	status = hv_el2_inject_exception(1, HV_EXC_IRQ, 1);
	assert(status == HV_OK);
	TEST_PASS("el2_inject_exception: inject irq to partition 1");

	/* Negative: Inject denied when exception type is disabled. */
	status = hv_el2_set_exception_enabled(HV_EXC_FIQ, 0);
	assert(status == HV_OK);
	status = hv_el2_inject_exception(1, HV_EXC_FIQ, 0);
	assert(status == HV_EPERM);
	TEST_PASS(
		"el2_inject_exception: denied when exception type is disabled");

	/* Re-enable and confirm injection works again. */
	status = hv_el2_set_exception_enabled(HV_EXC_FIQ, 1);
	assert(status == HV_OK);
	status = hv_el2_inject_exception(1, HV_EXC_FIQ, 0);
	assert(status == HV_OK);
	TEST_PASS("el2_inject_exception: allowed after re-enable");

	/* Negative: Invalid partition. */
	status = hv_el2_inject_exception(256, HV_EXC_SYNC, 0);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_inject_exception: rejects invalid partition");

	/* Negative: Invalid exception type. */
	status = hv_el2_inject_exception(0, 10, 0);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_inject_exception: rejects invalid exception type");

	/* Negative: Invalid vector. */
	status = hv_el2_inject_exception(0, HV_EXC_SYNC, 32);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_inject_exception: rejects invalid vector");
}

/**
 * EX2 / owner decision E3: classification of guest synchronous exceptions.
 * Data aborts are skipped; instruction aborts stop the partition; anything
 * that is not a direct stage-2 abort gets normal handling.
 */
static void test_s2_fault_action(void)
{
#define ESR(ec, iss) (((hv_u64)(ec) << 26) | (hv_u64)(iss))
	/* Data abort from lower EL, permission fault level 3 / 2. */
	assert(hv_s2_fault_action(ESR(0x24, 0x0F)) == HV_S2F_SKIP);
	assert(hv_s2_fault_action(ESR(0x24, 0x0E)) == HV_S2F_SKIP);
	/* Data abort, translation fault level 1. */
	assert(hv_s2_fault_action(ESR(0x24, 0x05)) == HV_S2F_SKIP);
	TEST_PASS("s2_fault_action: data aborts are skipped");

	/* Instruction abort from lower EL: permission (XN) and translation. */
	assert(hv_s2_fault_action(ESR(0x20, 0x0F)) == HV_S2F_STOP);
	assert(hv_s2_fault_action(ESR(0x20, 0x0D)) == HV_S2F_STOP);
	assert(hv_s2_fault_action(ESR(0x20, 0x07)) == HV_S2F_STOP);
	TEST_PASS("s2_fault_action: instruction aborts stop the partition");

	/* Stage-1 page-table-walk faults (S1PTW, ISS bit 7) are not direct. */
	assert(hv_s2_fault_action(ESR(0x24, (1U << 7) | 0x0F)) ==
	       HV_S2F_NOT_S2);
	assert(hv_s2_fault_action(ESR(0x20, (1U << 7) | 0x0F)) ==
	       HV_S2F_NOT_S2);
	/* FSC outside 0x04-0x0F (address size 0x03, sync external 0x10). */
	assert(hv_s2_fault_action(ESR(0x24, 0x03)) == HV_S2F_NOT_S2);
	assert(hv_s2_fault_action(ESR(0x24, 0x10)) == HV_S2F_NOT_S2);
	/* Other exception classes: HVC (0x16), same-EL aborts (0x21, 0x25). */
	assert(hv_s2_fault_action(ESR(0x16, 0x00)) == HV_S2F_NOT_S2);
	assert(hv_s2_fault_action(ESR(0x21, 0x0F)) == HV_S2F_NOT_S2);
	assert(hv_s2_fault_action(ESR(0x25, 0x0F)) == HV_S2F_NOT_S2);
	TEST_PASS("s2_fault_action: other exceptions get normal handling");
#undef ESR
}

/**
 * Main test runner.
 */

static void test_denied_log_rate_limit(void)
{
	unsigned logged = 0;

	for (unsigned k = 0; k < 10000U; k++)
		logged += (unsigned)hv_denied_note(3U);
	/* first 16, then the 4096th and 8192nd */
	assert(logged == HV_DENIED_LOG_FIRST + 2U);
	assert(hv_denied_count(3U) == 10000U);
	assert(hv_denied_count(4U) == 0U);
	assert(hv_denied_note(99U) == 1); /* unknown CPU: always logged */
	printf("[PASS] denied-access log rate limit\n");
}

int main(void)
{
	test_denied_log_rate_limit();
	printf("\n=== EL2 Exception Handling Unit Tests ===\n\n");

	test_el2_init();
	test_el2_register_handler();
	test_el2_register_partition_handler();
	test_el2_route_irq();
	test_el2_unroute_irq();
	test_el2_get_context();
	test_el2_set_exception_enabled();
	test_el2_get_exception_count();
	test_el2_inject_exception();
	test_s2_fault_action();

	printf("\n=== All EL2 exception tests passed ===\n\n");
	return 0;
}
