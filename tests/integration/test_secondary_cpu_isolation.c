/**
 * Secondary CPU isolation policy test.
 *
 * Validates the full multi-partition launch policy sequence for a
 * two-partition AMP configuration: a Linux-class guest on CPUs 0-1
 * and an RTOS guest on CPU 2.
 *
 * Tests do NOT exercise ARM64 assembly or PSCI (that requires hardware).
 * They exercise the C policy layer (stage2, irq, budget) to confirm that
 * the two-partition launch configuration is internally consistent and that
 * all isolation invariants hold before the ERET path is taken.
 *
 * @file tests/integration/test_secondary_cpu_isolation.c
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>

#include <haven/budget_sched.h>
#include <haven/irq_ownership.h>
#include <haven/stage2.h>
#include <haven/timer.h>
#include <haven/types.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", (msg))
#define TEST_FAIL(msg)                        \
	do {                                  \
		printf("[FAIL] %s\n", (msg)); \
		assert(0);                    \
	} while (0)

/* -----------------------------------------------------------------------
 * QEMU virt memory layout mirrors src/core/partition.c definitions
 * ----------------------------------------------------------------------- */
#define HAVEN_LOAD_PA 0x80000000UL
#define HAVEN_SIZE (8UL * 1024 * 1024)
#define PART_A_PA_BASE (HAVEN_LOAD_PA + HAVEN_SIZE)
#define PART_A_IPA_BASE 0x40000000UL
#define PART_A_SIZE (512UL * 1024 * 1024)
#define PART_B_PA_BASE (PART_A_PA_BASE + PART_A_SIZE)
#define PART_B_IPA_BASE 0x60000000UL
#define PART_B_SIZE (64UL * 1024 * 1024)
#define QEMU_UART_BASE 0x09000000UL

#define PARTITION_A_ID 1U
#define PARTITION_B_ID 2U

/* -----------------------------------------------------------------------
 * T1: Full two-partition stage-2 mapping must succeed
 *
 * Verifies that Partition A (512 MB + UART MMIO) and Partition B (64 MB)
 * can be mapped simultaneously without PA overlap.
 * ----------------------------------------------------------------------- */
static void test_both_partitions_stage2_map(void)
{
	struct hv_mem_region a_regions[] = {
		{
			.ipa_base = PART_A_IPA_BASE,
			.pa_base = PART_A_PA_BASE,
			.size = PART_A_SIZE,
			.attrs = 0,
		},
		{
			/* PL011 UART - owned exclusively by Partition A */
			.ipa_base = QEMU_UART_BASE,
			.pa_base = QEMU_UART_BASE,
			.size = 0x1000UL,
			.attrs = 1,
		},
	};
	struct hv_partition_mem a_mem = {
		.partition_id = PARTITION_A_ID,
		.regions = a_regions,
		.region_count = 2U,
	};

	struct hv_mem_region b_regions[] = {
		{
			.ipa_base = PART_B_IPA_BASE,
			.pa_base = PART_B_PA_BASE,
			.size = PART_B_SIZE,
			.attrs = 0,
		},
	};
	struct hv_partition_mem b_mem = {
		.partition_id = PARTITION_B_ID,
		.regions = b_regions,
		.region_count = 1U,
	};

	assert(hv_stage2_init() == HV_OK);
	assert(hv_stage2_map_partition(&a_mem) == HV_OK);
	assert(hv_stage2_map_partition(&b_mem) == HV_OK);

	/* Both partitions are active - PA ranges must be disjoint */
	assert(hv_stage2_partition_contains_pa(PARTITION_A_ID, PART_A_PA_BASE,
					       0x1000UL) == HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_B_ID, PART_A_PA_BASE,
					       0x1000UL) != HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_B_ID, PART_B_PA_BASE,
					       0x1000UL) == HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_A_ID, PART_B_PA_BASE,
					       0x1000UL) != HV_OK);

	/* UART region belongs only to Partition A */
	assert(hv_stage2_partition_contains_pa(PARTITION_A_ID, QEMU_UART_BASE,
					       0x1000UL) == HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_B_ID, QEMU_UART_BASE,
					       0x1000UL) != HV_OK);

	hv_stage2_unmap_partition(PARTITION_A_ID);
	hv_stage2_unmap_partition(PARTITION_B_ID);
	TEST_PASS("T1: two-partition stage-2 map - disjoint PA regions");
}

/* -----------------------------------------------------------------------
 * T2: Partition B UART MMIO access must be denied
 *
 * Partition B has NO UART mapped.  Any B→UART PA check must return false.
 * This validates the spatial isolation invariant before the ERET path.
 * ----------------------------------------------------------------------- */
static void test_partition_b_no_uart(void)
{
	struct hv_mem_region b_regions[] = {
		{
			.ipa_base = PART_B_IPA_BASE,
			.pa_base = PART_B_PA_BASE,
			.size = PART_B_SIZE,
			.attrs = 0,
		},
	};
	struct hv_partition_mem b_mem = {
		.partition_id = PARTITION_B_ID,
		.regions = b_regions,
		.region_count = 1U,
	};

	assert(hv_stage2_init() == HV_OK);
	assert(hv_stage2_map_partition(&b_mem) == HV_OK);

	/* UART at 0x09000000 is NOT in Partition B's stage-2 map */
	assert(hv_stage2_partition_contains_pa(PARTITION_B_ID, QEMU_UART_BASE,
					       0x1000UL) != HV_OK);

	hv_stage2_unmap_partition(PARTITION_B_ID);
	TEST_PASS(
		"T2: Partition B has no UART MMIO mapped (spatial isolation)");
}

/* -----------------------------------------------------------------------
 * T3: IRQ ownership exclusivity - CPU 0 SPI for Partition A
 *
 * IRQ 33 (PL011 UART) assigned to Partition A on CPU 0.
 * A second assignment of the same IRQ to Partition B must be rejected.
 * ----------------------------------------------------------------------- */
static void test_irq_exclusivity(void)
{
	struct hv_irq_route a_irq = {
		.irq_id = 33U,
		.owner_partition_id = PARTITION_A_ID,
		.target_cpu = 0U,
	};
	struct hv_irq_route b_irq_steal = {
		.irq_id = 33U,
		.owner_partition_id = PARTITION_B_ID,
		.target_cpu = 2U,
	};

	assert(hv_irq_owner_init() == HV_OK);
	assert(hv_irq_assign(&a_irq) == HV_OK);

	/* Partition B cannot steal IRQ 33 from Partition A */
	assert(hv_irq_assign(&b_irq_steal) == HV_EPERM);

	hv_irq_revoke(33U, PARTITION_A_ID);
	TEST_PASS("T3: IRQ 33 exclusivity - Partition B steal rejected");
}

/* -----------------------------------------------------------------------
 * T4: CPU budget partition - A+B budgets must each be ≤ their period
 *
 * Validates the AMP-specific budget split: 8ms/10ms for A (80%)
 * and 2ms/10ms for B (20%).  Both satisfy the budget ≤ period invariant.
 * ----------------------------------------------------------------------- */
static void test_cpu_budget_partition(void)
{
	struct hv_budget a_budget = {
		.partition_id = PARTITION_A_ID,
		.period_ns = 10000000ULL, /* 10 ms */
		.budget_ns = 8000000ULL, /*  8 ms */
	};
	struct hv_budget b_budget = {
		.partition_id = PARTITION_B_ID,
		.period_ns = 10000000ULL, /* 10 ms */
		.budget_ns = 2000000ULL, /*  2 ms */
	};

	assert(hv_budget_sched_init() == HV_OK);
	assert(hv_budget_set(&a_budget) == HV_OK);
	assert(hv_budget_set(&b_budget) == HV_OK);

	/* Each budget must be individually consumable without conflict */
	assert(hv_budget_consume(PARTITION_A_ID, 7000000ULL) == HV_OK);
	assert(hv_budget_consume(PARTITION_A_ID, 900000ULL) == HV_OK);
	/* A's budget (8ms) is now exhausted: next consume must be denied */
	assert(hv_budget_consume(PARTITION_A_ID, 200000ULL) == HV_EPERM);

	/* Partition B's budget is independent */
	assert(hv_budget_consume(PARTITION_B_ID, 1500000ULL) == HV_OK);
	assert(hv_budget_consume(PARTITION_B_ID, 400000ULL) == HV_OK);
	/* B's budget (2ms) now exhausted */
	assert(hv_budget_consume(PARTITION_B_ID, 200000ULL) == HV_EPERM);

	TEST_PASS(
		"T4: CPU budget partition - A=8ms/10ms, B=2ms/10ms, independent");
}

/* -----------------------------------------------------------------------
 * T5: Budget boundary - zero or over-period budget must be rejected
 *
 * Confirms that misconfigured RTOS budgets (budget > period or zero) are
 * caught before any ERET is issued, maintaining the fail-closed invariant.
 * Note: budget == period is permitted (partition monopolises its period).
 * ----------------------------------------------------------------------- */
static void test_budget_boundary_rejection(void)
{
	struct hv_budget bad_zero = {
		.partition_id = PARTITION_B_ID,
		.period_ns = 1000000ULL,
		.budget_ns = 0ULL, /* zero budget: must fail */
	};
	struct hv_budget bad_over = {
		.partition_id = PARTITION_B_ID,
		.period_ns = 1000000ULL,
		.budget_ns = 2000000ULL, /* budget > period: must fail */
	};
	struct hv_budget ok_equal = {
		.partition_id = PARTITION_B_ID,
		.period_ns = 1000000ULL,
		.budget_ns = 1000000ULL, /* budget == period: allowed */
	};

	assert(hv_budget_sched_init() == HV_OK);
	assert(hv_budget_set(&bad_zero) == HV_EINVAL);
	assert(hv_budget_set(&bad_over) == HV_EINVAL);
	assert(hv_budget_set(&ok_equal) == HV_OK); /* full-period monopoly OK */

	TEST_PASS(
		"T5: Invalid budgets (zero/over-period) rejected; equal-period allowed");
}

/* -----------------------------------------------------------------------
 * T6: Full secondary-CPU launch policy sequence
 *
 * Simulates the entire two-partition launch sequence that partitions_launch()
 * performs before the ERET path, confirming the policy state is correct
 * for both partitions at the C level.
 * ----------------------------------------------------------------------- */
static void test_full_secondary_launch_sequence(void)
{
	hv_status_t st;

	/* Stage-2 */
	struct hv_mem_region a_r[] = {
		{.ipa_base = PART_A_IPA_BASE,
		 .pa_base = PART_A_PA_BASE,
		 .size = PART_A_SIZE,
		 .attrs = 0},
		{.ipa_base = QEMU_UART_BASE,
		 .pa_base = QEMU_UART_BASE,
		 .size = 0x1000UL,
		 .attrs = 1},
	};
	struct hv_mem_region b_r[] = {
		{.ipa_base = PART_B_IPA_BASE,
		 .pa_base = PART_B_PA_BASE,
		 .size = PART_B_SIZE,
		 .attrs = 0},
	};
	struct hv_partition_mem a_mem = {.partition_id = PARTITION_A_ID,
					 .regions = a_r,
					 .region_count = 2U};
	struct hv_partition_mem b_mem = {.partition_id = PARTITION_B_ID,
					 .regions = b_r,
					 .region_count = 1U};

	/* IRQ */
	struct hv_irq_route a_irq = {.irq_id = 33U,
				     .owner_partition_id = PARTITION_A_ID,
				     .target_cpu = 0U};

	/* Budgets */
	struct hv_budget a_bud = {.partition_id = PARTITION_A_ID,
				  .period_ns = 10000000ULL,
				  .budget_ns = 8000000ULL};
	struct hv_budget b_bud = {.partition_id = PARTITION_B_ID,
				  .period_ns = 10000000ULL,
				  .budget_ns = 2000000ULL};

	/* Init all subsystems */
	assert(hv_stage2_init() == HV_OK);
	assert(hv_irq_owner_init() == HV_OK);
	assert(hv_budget_sched_init() == HV_OK);

	/* Map both partitions */
	st = hv_stage2_map_partition(&a_mem);
	assert(st == HV_OK);
	st = hv_stage2_map_partition(&b_mem);
	assert(st == HV_OK);

	/* Assign IRQs */
	assert(hv_irq_assign(&a_irq) == HV_OK);

	/* Set budgets */
	assert(hv_budget_set(&a_bud) == HV_OK);
	assert(hv_budget_set(&b_bud) == HV_OK);

	/* Verify final isolation state */
	assert(hv_stage2_partition_contains_pa(PARTITION_A_ID, PART_A_PA_BASE,
					       0x1000UL) == HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_B_ID, PART_B_PA_BASE,
					       0x1000UL) == HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_B_ID, PART_A_PA_BASE,
					       0x1000UL) != HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_A_ID, PART_B_PA_BASE,
					       0x1000UL) != HV_OK);
	assert(hv_stage2_partition_contains_pa(PARTITION_B_ID, QEMU_UART_BASE,
					       0x1000UL) != HV_OK);

	/* Budget state: both partitions still have their full allocations */
	assert(hv_budget_consume(PARTITION_A_ID, 1000000ULL) == HV_OK);
	assert(hv_budget_consume(PARTITION_B_ID, 500000ULL) == HV_OK);

	/* Tear down */
	hv_irq_revoke(33U, PARTITION_A_ID);
	hv_stage2_unmap_partition(PARTITION_A_ID);
	hv_stage2_unmap_partition(PARTITION_B_ID);

	TEST_PASS(
		"T6: Full secondary CPU launch sequence - policy state correct");
}

int main(void)
{
	printf("=== secondary CPU isolation policy tests ===\n");

	test_both_partitions_stage2_map();
	test_partition_b_no_uart();
	test_irq_exclusivity();
	test_cpu_budget_partition();
	test_budget_boundary_rejection();
	test_full_secondary_launch_sequence();

	printf("=== all 6 secondary CPU isolation tests PASS ===\n");
	return 0;
}
