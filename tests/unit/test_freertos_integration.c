/**
 * Unit tests for FreeRTOS guest integration.
 *
 * Tests cover:
 *   - Partition creation and configuration
 *   - Task registration and tracking
 *   - Shared memory allocation
 *   - Timer interrupt delivery
 *   - IRQ routing for RTOS handlers
 *   - Statistics tracking
 *
 * @file tests/unit/test_freertos_integration.c
 */

#include <assert.h>
#include <haven/freertos_integration.h>
#include <stdio.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg)                      \
	do {                                \
		printf("[FAIL] %s\n", msg); \
		assert(0);                  \
	} while (0)

/**
 * Test: Initialize FreeRTOS integration.
 */
static void test_freertos_init(void)
{
	hv_status_t status = hv_freertos_init();
	assert(status == HV_OK);
	TEST_PASS("freertos_init: basic initialization");
}

/**
 * Test: Create FreeRTOS partition.
 */
static void test_freertos_create_partition(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 32,
		.timer_frequency = 1000,
	};

	/* Create partition. */
	status = hv_freertos_create_partition(&config, &part_id);
	assert(status == HV_OK);
	assert(part_id == 0);
	TEST_PASS("freertos_create_partition: basic creation");

	/* Create another partition. */
	config.partition = 1;
	status = hv_freertos_create_partition(&config, &part_id);
	assert(status == HV_OK);
	assert(part_id == 1);
	TEST_PASS("freertos_create_partition: multiple partitions");

	/* Negative: NULL config. */
	status = hv_freertos_create_partition(NULL, &part_id);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_create_partition: rejects NULL config");

	/* Negative: NULL partition_id. */
	status = hv_freertos_create_partition(&config, NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_create_partition: rejects NULL partition_id");

	/* Negative: Invalid partition number in config. */
	config.partition = 256;
	status = hv_freertos_create_partition(&config, &part_id);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_create_partition: rejects invalid partition");
}

/**
 * Test: Register FreeRTOS task.
 */
static void test_freertos_register_task(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};

	hv_freertos_create_partition(&config, &part_id);

	/* Register task. */
	status = hv_freertos_register_task(part_id, 1, 10, 0x1000);
	assert(status == HV_OK);
	TEST_PASS("freertos_register_task: task registered");

	/* Register multiple tasks. */
	status = hv_freertos_register_task(part_id, 2, 20, 0x2000);
	assert(status == HV_OK);
	status = hv_freertos_register_task(part_id, 3, 30, 0x3000);
	assert(status == HV_OK);
	TEST_PASS("freertos_register_task: multiple tasks");

	/* Negative: Invalid partition. */
	status = hv_freertos_register_task(99, 4, 10, 0x4000);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_register_task: rejects invalid partition");

	/* Negative: Task ID 0. */
	status = hv_freertos_register_task(part_id, 0, 10, 0x5000);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_register_task: rejects task ID 0");
}

/**
 * Test: Save/restore task context.
 */
static void test_freertos_context_operations(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};

	hv_freertos_create_partition(&config, &part_id);
	hv_freertos_register_task(part_id, 1, 10, 0x1000);

	hv_task_context_t ctx = {
		.task_id = 1,
		.priority = 10,
		.state = HV_TASK_READY,
		.sp = 0x1000,
		.pc = 0x80000000,
	};

	/* First restore to move the task to RUNNING state. */
	status = hv_freertos_restore_context(part_id, &ctx);
	assert(status == HV_OK);
	TEST_PASS("freertos_context: restore (READY→RUNNING) succeeds");

	/* Now save is valid - task is RUNNING. */
	status = hv_freertos_save_context(part_id, &ctx);
	assert(status == HV_OK);
	TEST_PASS("freertos_context: save (RUNNING→READY) succeeds");

	/* Negative: NULL context. */
	status = hv_freertos_save_context(part_id, NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_context: rejects NULL context");

	/* Negative: Invalid partition. */
	status = hv_freertos_save_context(99, &ctx);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_context: rejects invalid partition");
}

/**
 * Test: Allocate shared memory region.
 */
static void test_freertos_shared_memory(void)
{
	hv_freertos_init();

	hv_u32 part1, part2, region_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};

	/* Create two partitions. */
	config.partition = 0;
	hv_freertos_create_partition(&config, &part1);
	config.partition = 1;
	hv_freertos_create_partition(&config, &part2);

	/* Allocate shared region. */
	status = hv_freertos_allocate_shared_region(part1, part2, 0x1000,
						    HV_SHARED_RW, &region_id);
	assert(status == HV_OK);
	assert(region_id == 0);
	TEST_PASS("freertos_shared_memory: allocation succeeds");

	/* Get shared address for partition 1. */
	hv_u64 addr;
	status = hv_freertos_get_shared_address(region_id, part1, &addr);
	assert(status == HV_OK);
	assert(addr > 0);
	TEST_PASS("freertos_shared_memory: get address succeeds");

	/* Get shared address for partition 2. */
	status = hv_freertos_get_shared_address(region_id, part2, &addr);
	assert(status == HV_OK);
	TEST_PASS("freertos_shared_memory: both partitions can access");

	/* Negative: Misaligned size. */
	status = hv_freertos_allocate_shared_region(part1, part2, 0x1001,
						    HV_SHARED_RW, &region_id);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_shared_memory: rejects misaligned size");

	/* Negative: Access for unauthorized partition. */
	status = hv_freertos_get_shared_address(region_id, 99, &addr);
	assert(status == HV_EPERM);
	TEST_PASS("freertos_shared_memory: denies unauthorized access");
}

/**
 * Test: Deliver timer interrupts.
 */
static void test_freertos_timer_delivery(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};

	hv_freertos_create_partition(&config, &part_id);

	/* Deliver timer. */
	status = hv_freertos_deliver_timer(part_id);
	assert(status == HV_OK);
	TEST_PASS("freertos_timer: delivery succeeds");

	/* Deliver multiple timers. */
	for (int i = 0; i < 100; i++) {
		status = hv_freertos_deliver_timer(part_id);
		assert(status == HV_OK);
	}
	TEST_PASS("freertos_timer: multiple deliveries");

	/* Negative: Invalid partition. */
	status = hv_freertos_deliver_timer(99);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_timer: rejects invalid partition");
}

/**
 * Test: IRQ routing for FreeRTOS.
 */
static void test_freertos_irq_routing(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};

	hv_freertos_create_partition(&config, &part_id);

	/* Route IRQ. */
	status = hv_freertos_route_irq(part_id, 10);
	assert(status == HV_OK);
	TEST_PASS("freertos_irq: route succeeds");

	/* Unroute IRQ. */
	status = hv_freertos_unroute_irq(part_id, 10);
	assert(status == HV_OK);
	TEST_PASS("freertos_irq: unroute succeeds");

	/* Negative: Invalid partition. */
	status = hv_freertos_route_irq(99, 10);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_irq: rejects invalid partition");

	/* Negative: Invalid IRQ. */
	status = hv_freertos_route_irq(part_id, 256);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_irq: rejects invalid irq");
}

/**
 * Test: Partition destruction.
 */
static void test_freertos_destroy_partition(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};

	hv_freertos_create_partition(&config, &part_id);
	hv_freertos_register_task(part_id, 1, 10, 0x1000);

	/* Destroy partition. */
	status = hv_freertos_destroy_partition(part_id);
	assert(status == HV_OK);
	TEST_PASS("freertos_destroy: destruction succeeds");

	/* Negative: Double destroy. */
	status = hv_freertos_destroy_partition(part_id);
	assert(status == HV_EPERM);
	TEST_PASS("freertos_destroy: denies double destroy");

	/* Negative: Invalid partition. */
	status = hv_freertos_destroy_partition(99);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_destroy: rejects invalid partition");
}

/**
 * Test: Statistics tracking.
 */
static void test_freertos_statistics(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_status_t status;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};

	hv_freertos_create_partition(&config, &part_id);
	hv_freertos_register_task(part_id, 1, 10, 0x1000);
	hv_freertos_register_task(part_id, 2, 20, 0x2000);

	/* Deliver some timers. */
	for (int i = 0; i < 50; i++) {
		hv_freertos_deliver_timer(part_id);
	}

	/* Move task 1 to RUNNING so save_context succeeds. */
	hv_task_context_t ctx = {.task_id = 1};
	hv_freertos_restore_context(part_id, &ctx); /* READY → RUNNING */
	hv_freertos_save_context(part_id,
				 &ctx); /* RUNNING → READY, ++ctx_switches */

	/* Get statistics. */
	hv_u32 task_count;
	hv_u64 context_switches, timer_ticks;
	status = hv_freertos_get_stats(part_id, &task_count, &context_switches,
				       &timer_ticks);
	assert(status == HV_OK);
	assert(task_count == 2);
	assert(context_switches >= 1);
	assert(timer_ticks == 50);
	TEST_PASS("freertos_statistics: stats retrieved correctly");

	/* Negative: NULL output pointers. */
	status = hv_freertos_get_stats(part_id, NULL, &context_switches,
				       &timer_ticks);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_statistics: rejects NULL pointers");

	/* Negative: Invalid partition. */
	status = hv_freertos_get_stats(99, &task_count, &context_switches,
				       &timer_ticks);
	assert(status == HV_EINVAL);
	TEST_PASS("freertos_statistics: rejects invalid partition");
}

/**
 * Test: Context state-machine hardening.
 *
 * save_context must fail unless the task is in RUNNING state.
 * restore_context must fail for BLOCKED/DELETED tasks.
 */
static void test_context_state_machine(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};
	hv_freertos_create_partition(&config, &part_id);
	/* Register task - initial state is READY, not RUNNING */
	hv_freertos_register_task(part_id, 1, 10, 0x1000);

	hv_task_context_t ctx = {.task_id = 1,
				 .sp = 0x1000,
				 .pc = 0x80000000,
				 .state = HV_TASK_READY};

	/* save must fail: task is READY, not RUNNING */
	assert(hv_freertos_save_context(part_id, &ctx) == HV_EPERM);
	TEST_PASS("context_state_machine: save blocked for READY task");

	/* restore must succeed: task is READY → moves to RUNNING */
	assert(hv_freertos_restore_context(part_id, &ctx) == HV_OK);
	TEST_PASS("context_state_machine: restore succeeds for READY task");

	/* Now task is RUNNING - save must succeed */
	assert(hv_freertos_save_context(part_id, &ctx) == HV_OK);
	TEST_PASS("context_state_machine: save succeeds for RUNNING task");

	/* After save, task is READY. Block it. */
	assert(hv_freertos_task_block(part_id, 1) == HV_OK);

	/* restore must fail for BLOCKED task */
	assert(hv_freertos_restore_context(part_id, &ctx) == HV_EPERM);
	TEST_PASS("context_state_machine: restore blocked for BLOCKED task");
}

/**
 * Test: Max-task boundary enforcement.
 *
 * Registers exactly max_tasks tasks; the next one must be rejected.
 */
static void test_max_tasks_boundary(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 3,
		.timer_frequency = 1000,
	};
	hv_freertos_create_partition(&config, &part_id);

	assert(hv_freertos_register_task(part_id, 1, 10, 0x1000) == HV_OK);
	assert(hv_freertos_register_task(part_id, 2, 20, 0x2000) == HV_OK);
	assert(hv_freertos_register_task(part_id, 3, 30, 0x3000) == HV_OK);

	/* max_tasks = 3 is now full */
	assert(hv_freertos_register_task(part_id, 4, 40, 0x4000) == HV_ENOSPC);
	TEST_PASS("max_tasks_boundary: (max+1)th task rejected with HV_ENOSPC");
}

/**
 * Test: Task block / unblock cycle.
 *
 * Validates full RUNNING → BLOCKED → READY state transitions.
 */
static void test_task_block_unblock(void)
{
	hv_freertos_init();

	hv_u32 part_id;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};
	hv_freertos_create_partition(&config, &part_id);
	hv_freertos_register_task(part_id, 1, 10, 0x1000);

	hv_task_context_t ctx = {.task_id = 1, .sp = 0x1000, .pc = 0x80000000};

	/* Move task to RUNNING */
	assert(hv_freertos_restore_context(part_id, &ctx) == HV_OK);

	/* Block it */
	assert(hv_freertos_task_block(part_id, 1) == HV_OK);
	/* Double-block must fail */
	assert(hv_freertos_task_block(part_id, 1) == HV_EPERM);
	TEST_PASS(
		"task_block_unblock: block RUNNING task; double-block rejected");

	/* Unblock restores READY */
	assert(hv_freertos_task_unblock(part_id, 1) == HV_OK);
	/* Double-unblock must fail */
	assert(hv_freertos_task_unblock(part_id, 1) == HV_EPERM);
	TEST_PASS(
		"task_block_unblock: unblock BLOCKED task; double-unblock rejected");

	/* Non-existent task */
	assert(hv_freertos_task_block(part_id, 99) == HV_EINVAL);
	assert(hv_freertos_task_unblock(part_id, 99) == HV_EINVAL);
	TEST_PASS("task_block_unblock: invalid task_id rejected");
}

/**
 * Test: Shared region exact-boundary handling.
 *
 * Zero-size and non-page-aligned regions must be rejected.
 * A single-page region at exactly 0x1000 must be accepted.
 */
static void test_shared_region_boundary(void)
{
	hv_freertos_init();

	hv_u32 p1, p2, rid;
	hv_freertos_config_t config = {
		.partition = 0,
		.cpu_cores = 1,
		.time_budget_us = 10000,
		.max_tasks = 4,
		.timer_frequency = 1000,
	};
	config.partition = 0;
	hv_freertos_create_partition(&config, &p1);
	config.partition = 1;
	hv_freertos_create_partition(&config, &p2);

	/* Zero size - rejected */
	assert(hv_freertos_allocate_shared_region(p1, p2, 0, HV_SHARED_RW,
						  &rid) == HV_EINVAL);
	TEST_PASS("shared_region_boundary: zero size rejected");

	/* Non-aligned - rejected */
	assert(hv_freertos_allocate_shared_region(p1, p2, 0x0FFF, HV_SHARED_RW,
						  &rid) == HV_EINVAL);
	TEST_PASS("shared_region_boundary: non-page-aligned size rejected");

	/* Exactly one page - accepted */
	assert(hv_freertos_allocate_shared_region(p1, p2, 0x1000, HV_SHARED_RO,
						  &rid) == HV_OK);
	TEST_PASS("shared_region_boundary: exact single-page region accepted");

	/* Third-party access denied */
	hv_u64 addr;
	assert(hv_freertos_get_shared_address(rid, 2, &addr) == HV_EPERM);
	TEST_PASS(
		"shared_region_boundary: third-party partition access denied");
}

/**
 * Main test runner.
 */
int main(void)
{
	printf("\n=== FreeRTOS Integration Unit Tests ===\n\n");

	test_freertos_init();
	test_freertos_create_partition();
	test_freertos_register_task();
	test_freertos_context_operations();
	test_freertos_shared_memory();
	test_freertos_timer_delivery();
	test_freertos_irq_routing();
	test_freertos_destroy_partition();
	test_freertos_statistics();

	/* Hardening tests added in feat/freertos-context-hardening */
	test_context_state_machine();
	test_max_tasks_boundary();
	test_task_block_unblock();
	test_shared_region_boundary();

	printf("\n=== All FreeRTOS integration tests passed ===\n\n");
	return 0;
}
