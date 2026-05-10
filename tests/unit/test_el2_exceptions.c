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

#include <haven/el2_exceptions.h>
#include <stdio.h>
#include <assert.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg) do { printf("[FAIL] %s\n", msg); assert(0); } while(0)

/**
 * Test handler that records calls.
 */
static int test_handler_called = 0;

static hv_status_t test_handler(hv_exc_context_t *ctx) {
	(void)ctx;  /* Mark unused parameter */
	test_handler_called++;
	return HV_OK;
}

/**
 * Test: Initialize EL2 exception handling.
 */
static void test_el2_init(void) {
	hv_status_t status = hv_el2_exceptions_init();
	assert(status == HV_OK);
	TEST_PASS("el2_init: basic initialization");
}

/**
 * Test: Register global exception handlers.
 */
static void test_el2_register_handler(void) {
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
static void test_el2_register_partition_handler(void) {
	hv_el2_exceptions_init();

	hv_status_t status;

	/* Register partition 0, SYNC exception handler. */
	status = hv_el2_register_partition_handler(0, HV_EXC_SYNC, test_handler);
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
	status = hv_el2_register_partition_handler(256, HV_EXC_SYNC, test_handler);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_register_partition_handler: rejects invalid partition");

	/* Negative: Invalid exception type. */
	status = hv_el2_register_partition_handler(0, 10, test_handler);
	assert(status == HV_EINVAL);
	TEST_PASS("el2_register_partition_handler: rejects invalid exception type");
}

/**
 * Test: Route IRQs to partitions.
 */
static void test_el2_route_irq(void) {
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
static void test_el2_unroute_irq(void) {
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
static void test_el2_get_context(void) {
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
static void test_el2_set_exception_enabled(void) {
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
static void test_el2_get_exception_count(void) {
	hv_el2_exceptions_init();

	hv_u64 count;

	/* Get SYNC exception count (should be 0 initially). */
	count = hv_el2_get_exception_count(HV_EXC_SYNC);
	assert(count == 0);
	TEST_PASS("el2_get_exception_count: initial count is 0");

	/* Negative: Invalid exception type. */
	count = hv_el2_get_exception_count(10);
	assert(count == (hv_u64)-1);
	TEST_PASS("el2_get_exception_count: returns -1 for invalid type");
}

/**
 * Test: Inject exception into partition.
 */
static void test_el2_inject_exception(void) {
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
 * Main test runner.
 */
int main(void) {
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

	printf("\n=== All EL2 exception tests passed ===\n\n");
	return 0;
}
