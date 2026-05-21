/* SPDX-License-Identifier: Apache-2.0 */
/*
 * src/core/partition.c - Haven static partition launcher.
 *
 * Called from haven_init() in src/core/init.c after all isolation
 * policy modules are initialized.  Sets up memory, IRQs, and CPU
 * time budgets for two partitions, then on ARM64 performs ERET into
 * partition A's entry point.
 *
 * On host (unit-test) builds HAVEN_ARCH_ARM64 is not defined; the
 * function validates the policy configuration and returns without
 * launching any real guest.
 *
 * Ownership summary (Phase 3 / QEMU virt):
 *   Partition A  - Linux-class guest, CPUs 0-1, 512 MB, 8/10 ms budget
 *   Partition B  - RTOS guest,        CPU  2,  64 MB,  2/10 ms budget
 *
 * Hardware assumption: all addresses below are QEMU virt defaults.
 *   Override at build time by defining HAVEN_PLATFORM_IMX95_DEVKIT
 *   (Phase 4) or similar to pull in a different memory.h.
 */

#include <haven/budget_sched.h>
#include <haven/irq_ownership.h>
#include <haven/stage2.h>
#include <haven/types.h>
#include <stdint.h>

/* Platform memory layout - QEMU virt default.
 * The -I. flag in ARM64_CFLAGS makes the root the first extra search
 * directory, so "src/platform/..." resolves from the repository root. */
#include "src/platform/qemu-virt/memory.h"

/* -----------------------------------------------------------------------
 * Partition IDs - must be non-zero and unique (stage2.c contract)
 * ----------------------------------------------------------------------- */

#define PARTITION_A_ID 1U /* Linux-class guest */
#define PARTITION_B_ID 2U /* RTOS guest        */

/* -----------------------------------------------------------------------
 * Stage-2 memory regions
 *
 * Each partition has its own stage-2 table; they may share the same IPA
 * range (the current stage2.c only enforces *PA* non-overlap).
 *
 * Partition A: 512 MB  PA [0x40400000, 0x60400000)  IPA 0x40000000
 * Partition B:  64 MB  PA [0x60400000, 0x64400000)  IPA 0x40000000
 * ----------------------------------------------------------------------- */

static const struct hv_mem_region part_a_regions[] = {
	{
		.ipa_base = PART_A_IPA_BASE,
		.pa_base = PART_A_PA_BASE,
		.size = PART_A_SIZE,
		.attrs = 0, /* normal cacheable, full access */
	},
	/* PL011 UART0 MMIO - identity-mapped so the guest stub can print.
	 * The device is owned by partition A; partition B has no UART mapping,
	 * so a B→UART access would fault at stage-2. */
	{
		.ipa_base = QEMU_UART_BASE,
		.pa_base = QEMU_UART_BASE,
		.size = 0x1000UL, /* 4 KB MMIO page */
		.attrs = 1, /* device memory (nGnRE) */
	},
};

static const struct hv_mem_region part_b_regions[] = {
	{
		.ipa_base = PART_B_IPA_BASE,
		.pa_base = PART_B_PA_BASE,
		.size = PART_B_SIZE,
		.attrs = 0,
	},
};

static const struct hv_partition_mem part_a_mem_cfg = {
	.partition_id = PARTITION_A_ID,
	.regions = part_a_regions,
	.region_count = 2U, /* DRAM + UART MMIO */
};

static const struct hv_partition_mem part_b_mem_cfg = {
	.partition_id = PARTITION_B_ID,
	.regions = part_b_regions,
	.region_count = 1U,
};

/* -----------------------------------------------------------------------
 * IRQ routing
 *
 * GIC SPI numbers (QEMU virt, aarch64):
 *   IRQ 33 - PL011 UART0  (SPI, routable to Partition A)
 *   IRQ 26 - virtual RTOS timer (SPI, routable to Partition B, CPU 2)
 *
 * IRQ 27 (arch timer PPI) is NOT listed: PPIs (16-31) are per-CPU and
 * cannot be routed via GICD_IROUTER; they fire on whichever CPU the
 * partition runs and need no explicit ownership entry.
 *
 * Interrupt ownership is exclusive: once assigned to a partition no
 * other partition can claim the same IRQ (hv_irq_assign contract).
 * ----------------------------------------------------------------------- */

static const struct hv_irq_route part_a_irqs[] = {
	{.irq_id = 33U, .owner_partition_id = PARTITION_A_ID, .target_cpu = 0U},
};

/* Partition B: no SPI IRQ assignments for the QEMU demo.
 * The RTOS partition uses its per-CPU virtual timer PPI (IRQ 27)
 * which fires automatically on CPU 2 without explicit GIC routing. */

/* -----------------------------------------------------------------------
 * CPU time budgets  (10 ms period, static allocation)
 *
 *   Partition A: 8 ms / 10 ms  (80% - Linux-class latency-tolerant)
 *   Partition B: 2 ms / 10 ms  (20% - RTOS hard-deadline)
 *
 * Both period_ns > budget_ns satisfies the hv_budget_set() pre-condition.
 * ----------------------------------------------------------------------- */

static const struct hv_budget part_a_budget = {
	.partition_id = PARTITION_A_ID,
	.period_ns = 10000000ULL, /* 10 ms */
	.budget_ns = 8000000ULL, /*  8 ms */
};

static const struct hv_budget part_b_budget = {
	.partition_id = PARTITION_B_ID,
	.period_ns = 10000000ULL, /* 10 ms */
	.budget_ns = 2000000ULL, /*  2 ms */
};

/* -----------------------------------------------------------------------
 * ARM64 ERET stub - defined in arch/arm64/partition.S
 * Transfers control from EL2 to EL1 at the given entry PA.
 * This function does NOT return.
 * ----------------------------------------------------------------------- */

#ifdef HAVEN_ARCH_ARM64
extern void hv_arch_start_partition(uintptr_t entry, uintptr_t sp,
				    uintptr_t arg);
extern void hv_arch_stage2_enable(uint32_t partition_id, uint32_t vmid);
extern void hv_arch_secondary_start(uint32_t cpu_id,
				    void (*entry_fn)(uint64_t cpu_id));
extern int hv_arch_psci_cpu_on(uint32_t cpu_id, uintptr_t entry_pa);
/* Symbol defined in arch/arm64/boot.S - the secondary CPU spin-loop entry */
extern void secondary_entry(void);

/*
 * partition_b_secondary_cpu_init - runs on CPU 2 after PSCI wakeup.
 *
 * Enables stage-2 translation for Partition B and ERETSs into
 * Partition B's EL1 entry point.  This function never returns.
 */
__attribute__((noreturn)) static void
partition_b_secondary_cpu_init(uint64_t cpu_id)
{
	(void)cpu_id;

	hv_arch_stage2_enable(PARTITION_B_ID, PARTITION_B_ID);

	/* ERET to Partition B entry at its IPA base. */
	hv_arch_start_partition(
		(uintptr_t)PART_B_IPA_BASE,
		(uintptr_t)(PART_B_IPA_BASE + PART_B_SIZE - 0x1000UL), 0UL);

	/* Unreachable */
	__builtin_unreachable();
}
#endif

extern void hv_panic(const char *msg);

/* -----------------------------------------------------------------------
 * partitions_launch - public API, called from haven_init()
 *
 * Steps:
 *   1. Map stage-2 memory regions for both partitions.
 *   2. Assign interrupt ownership.
 *   3. Program CPU time budgets.
 *   4. (ARM64 only) ERET into partition A.  hv_arch_start_partition()
 *      never returns; secondary CPUs are started by partition A later.
 *
 * On host builds the function returns after completing the policy steps
 * so that test binaries can validate the configuration without any
 * hardware interaction.
 * ----------------------------------------------------------------------- */

void partitions_launch(void)
{
	hv_u32 i;
	hv_status_t st;
	extern void hv_printk(const char *fmt, ...);

	hv_printk("HAVEN: launching 2-partition configuration\n");

	/* ----- Stage-2 memory mapping ------------------------------------- */

	st = hv_stage2_map_partition(&part_a_mem_cfg);
	if (st != HV_OK) {
		hv_panic("partition A stage-2 mapping failed");
	}
	hv_printk(
		"HAVEN: partition A mapped PA=0x%lx IPA=0x%lx size=%uMB + UART MMIO\n",
		(unsigned long)PART_A_PA_BASE, (unsigned long)PART_A_IPA_BASE,
		(unsigned)(PART_A_SIZE >> 20));

	st = hv_stage2_map_partition(&part_b_mem_cfg);
	if (st != HV_OK) {
		hv_stage2_unmap_partition(PARTITION_A_ID);
		hv_panic("partition B stage-2 mapping failed");
	}
	hv_printk("HAVEN: partition B mapped PA=0x%lx IPA=0x%lx size=%uMB\n",
		  (unsigned long)PART_B_PA_BASE, (unsigned long)PART_B_IPA_BASE,
		  (unsigned)(PART_B_SIZE >> 20));

	/* ----- IRQ ownership ---------------------------------------------- */

	for (i = 0U; i < sizeof(part_a_irqs) / sizeof(part_a_irqs[0]); ++i) {
		st = hv_irq_assign(&part_a_irqs[i]);
		if (st != HV_OK) {
			hv_panic("partition A IRQ assignment failed");
		}
	}
	hv_printk("HAVEN: partition A IRQs assigned (SPI 33 -> CPU 0)\n");

	/* Partition B has no SPI IRQs to assign in the QEMU demo. */

	/* ----- Scheduler budgets ------------------------------------------ */

	st = hv_budget_set(&part_a_budget);
	if (st != HV_OK) {
		hv_panic("partition A budget setup failed");
	}
	st = hv_budget_set(&part_b_budget);
	if (st != HV_OK) {
		hv_panic("partition B budget setup failed");
	}
	hv_printk("HAVEN: budgets set  A=%ums/%ums  B=%ums/%ums\n",
		  (unsigned)(part_a_budget.budget_ns / 1000000U),
		  (unsigned)(part_a_budget.period_ns / 1000000U),
		  (unsigned)(part_b_budget.budget_ns / 1000000U),
		  (unsigned)(part_b_budget.period_ns / 1000000U));

#ifdef HAVEN_ARCH_ARM64
	hv_printk("HAVEN: enabling stage-2 for partition A (VMID=%u)\n",
		  PARTITION_A_ID);
	hv_arch_stage2_enable(PARTITION_A_ID, PARTITION_A_ID);

	/* Bring up CPU 2 via PSCI and point it at Haven's secondary spin-loop.
	 * Once the mailbox is filled (hv_arch_secondary_start below), CPU 2
	 * will jump to partition_b_secondary_cpu_init and ERET into Partition B.
	 *
	 * The PSCI CPU_ON call is asynchronous: CPU 2 may still be waking up
	 * when we write the mailbox, so the SEV in hv_arch_secondary_start
	 * guarantees the wake-up is observed. */
	if (hv_arch_psci_cpu_on(2U, (uintptr_t)secondary_entry) == 0) {
		hv_arch_secondary_start(2U, partition_b_secondary_cpu_init);
		hv_printk("HAVEN: CPU 2 woken for partition B (VMID=%u)\n",
			  PARTITION_B_ID);
	} else {
		hv_printk(
			"HAVEN: PSCI CPU_ON failed for CPU 2 - partition B inactive\n");
	}

	hv_printk("HAVEN: ERET -> partition A entry IPA=0x%lx\n",
		  (unsigned long)PART_A_IPA_BASE);
	hv_arch_start_partition(
		(uintptr_t)PART_A_IPA_BASE,
		(uintptr_t)(PART_A_IPA_BASE + PART_A_SIZE - 0x1000UL), 0UL);

	/* Unreachable - ERET transfers control permanently */
#endif
}
