/**
 * Unit tests for per-partition timer deadline enforcement.
 *
 * Tests cover:
 *   - Initialization
 *   - Deadline set / check / acknowledge / cancel happy paths
 *   - Partition 0 and out-of-range rejection
 *   - Expired-before-acknowledged guard (fail-closed)
 *   - Cancel denied on expired deadline
 *   - Early acknowledge denial
 *   - One active deadline per partition
 *
 * @file tests/unit/test_timer.c
 */

#include <assert.h>
#include <haven/timer.h>
#include <stdio.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg)                      \
	do {                                \
		printf("[FAIL] %s\n", msg); \
		assert(0);                  \
	} while (0)

static void test_timer_init(void)
{
	hv_status_t s = hv_timer_init();
	assert(s == HV_OK);
	TEST_PASS("timer_init: basic initialization");
}

static void test_timer_set_deadline(void)
{
	hv_timer_init();

	hv_status_t s;

	/* Valid set for partition 1. */
	s = hv_timer_set_deadline(1, 1000000ULL);
	assert(s == HV_OK);
	TEST_PASS("timer_set_deadline: valid deadline accepted");

	/* Negative: partition 0 invalid. */
	s = hv_timer_set_deadline(0, 1000000ULL);
	assert(s == HV_EINVAL);
	TEST_PASS("timer_set_deadline: rejects partition 0");

	/* Negative: deadline_ns == 0 invalid. */
	s = hv_timer_set_deadline(2, 0);
	assert(s == HV_EINVAL);
	TEST_PASS("timer_set_deadline: rejects zero deadline");

	/* Negative: only one active deadline per partition. */
	s = hv_timer_set_deadline(1, 2000000ULL);
	assert(s == HV_ENOSPC);
	TEST_PASS("timer_set_deadline: rejects second deadline before cancel");
}

static void test_timer_check_deadline(void)
{
	hv_timer_init();

	hv_status_t s;
	int expired = -1;

	hv_timer_set_deadline(1, 5000ULL);

	/* Check before deadline - not expired. */
	s = hv_timer_check_deadline(1, 4000ULL, &expired);
	assert(s == HV_OK);
	assert(expired == 0);
	TEST_PASS("timer_check_deadline: not expired before deadline");

	/* Check at deadline - expired. */
	s = hv_timer_check_deadline(1, 5000ULL, &expired);
	assert(s == HV_OK);
	assert(expired == 1);
	TEST_PASS("timer_check_deadline: expired at exact deadline");

	/* Check past deadline - still expired. */
	s = hv_timer_check_deadline(1, 9000ULL, &expired);
	assert(s == HV_OK);
	assert(expired == 1);
	TEST_PASS("timer_check_deadline: still expired past deadline");

	/* Negative: partition 0. */
	s = hv_timer_check_deadline(0, 5000ULL, &expired);
	assert(s == HV_EINVAL);
	TEST_PASS("timer_check_deadline: rejects partition 0");

	/* Negative: now_ns == 0. */
	s = hv_timer_check_deadline(1, 0, &expired);
	assert(s == HV_EINVAL);
	TEST_PASS("timer_check_deadline: rejects zero now_ns");

	/* Negative: NULL expired output. */
	s = hv_timer_check_deadline(1, 5000ULL, NULL);
	assert(s == HV_EINVAL);
	TEST_PASS("timer_check_deadline: rejects NULL expired pointer");

	/* Negative: no active deadline. */
	s = hv_timer_check_deadline(2, 5000ULL, &expired);
	assert(s == HV_EPERM);
	TEST_PASS("timer_check_deadline: rejects partition with no deadline");
}

static void test_timer_acknowledge(void)
{
	hv_timer_init();

	hv_status_t s;
	int expired = -1;

	/* Set and expire a deadline. */
	hv_timer_set_deadline(1, 3000ULL);
	hv_timer_check_deadline(1, 5000ULL, &expired);
	assert(expired == 1);

	/* Acknowledge the expired deadline. */
	s = hv_timer_acknowledge(1);
	assert(s == HV_OK);
	TEST_PASS("timer_acknowledge: expired deadline acknowledged");

	/* After ack, can set a new deadline. */
	s = hv_timer_set_deadline(1, 10000ULL);
	assert(s == HV_OK);
	TEST_PASS("timer_acknowledge: new deadline allowed after ack");

	/* Negative: early acknowledgement denied. */
	s = hv_timer_acknowledge(1);
	assert(s == HV_EPERM);
	TEST_PASS("timer_acknowledge: denies early ack (not yet expired)");

	/* Negative: no active deadline. */
	s = hv_timer_acknowledge(2);
	assert(s == HV_EPERM);
	TEST_PASS("timer_acknowledge: rejects partition with no deadline");

	/* Negative: partition 0. */
	s = hv_timer_acknowledge(0);
	assert(s == HV_EINVAL);
	TEST_PASS("timer_acknowledge: rejects partition 0");
}

static void test_timer_cancel(void)
{
	hv_timer_init();

	hv_status_t s;
	int expired = -1;

	/* Set and cancel before expiry. */
	hv_timer_set_deadline(1, 9000ULL);
	s = hv_timer_cancel(1);
	assert(s == HV_OK);
	TEST_PASS("timer_cancel: cancel succeeds before expiry");

	/* After cancel, a new deadline can be set. */
	s = hv_timer_set_deadline(1, 9000ULL);
	assert(s == HV_OK);
	TEST_PASS("timer_cancel: new deadline allowed after cancel");

	/* Let the deadline expire. */
	hv_timer_check_deadline(1, 10000ULL, &expired);
	assert(expired == 1);

	/* Cancel after expiry must be denied (not a silent swallow). */
	s = hv_timer_cancel(1);
	assert(s == HV_EPERM);
	TEST_PASS("timer_cancel: denied on expired (unacknowledged) deadline");

	/* Negative: no active deadline. */
	hv_timer_acknowledge(1);
	s = hv_timer_cancel(1);
	assert(s == HV_EPERM);
	TEST_PASS("timer_cancel: rejects partition with no deadline");

	/* Negative: partition 0. */
	s = hv_timer_cancel(0);
	assert(s == HV_EINVAL);
	TEST_PASS("timer_cancel: rejects partition 0");
}

static void test_timer_expired_blocks_new_deadline(void)
{
	hv_timer_init();

	int expired = -1;

	/* Set, expire without ack, then try to set a new deadline. */
	hv_timer_set_deadline(3, 1000ULL);
	hv_timer_check_deadline(3, 2000ULL, &expired);
	assert(expired == 1);

	hv_status_t s = hv_timer_set_deadline(3, 5000ULL);
	assert(s == HV_EPERM);
	TEST_PASS(
		"timer_set_deadline: blocked by unacknowledged expiry (fail-closed)");

	/* Acknowledge then set succeeds. */
	hv_timer_acknowledge(3);
	s = hv_timer_set_deadline(3, 5000ULL);
	assert(s == HV_OK);
	TEST_PASS("timer_set_deadline: succeeds after acknowledgement");
}

int main(void)
{
	printf("\n=== Timer Deadline Enforcement Unit Tests ===\n\n");

	test_timer_init();
	test_timer_set_deadline();
	test_timer_check_deadline();
	test_timer_acknowledge();
	test_timer_cancel();
	test_timer_expired_blocks_new_deadline();

	printf("\n=== All timer tests passed ===\n\n");
	return 0;
}
