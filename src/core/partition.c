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
#include <haven/smmu.h>
#include <haven/stage2.h>
#include <haven/types.h>
#include <stdint.h>

#ifdef HAVEN_ARCH_ARM64
#include "drivers/pci/ecam.h"
#endif

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
 * Partition A: 512 MB  PA [0x80800000, 0xA0800000)  IPA 0x40000000
 *              2 MB    PA [0xA4C00000, 0xA4E00000)  IPA 0x70000000 (read-only)
 * Partition B:  64 MB  PA [0xA0800000, 0xA4800000)  IPA 0x60000000
 * (values for qemu-virt; see src/platform/qemu-virt/memory.h)
 * ----------------------------------------------------------------------- */

static const struct hv_mem_region part_a_regions[] = {
	{
		.ipa_base = PART_A_IPA_BASE,
		.pa_base = PART_A_PA_BASE,
		.size = PART_A_SIZE,
		.attrs = HV_MEM_RWX, /* Normal WB, read/write/execute */
	},
	/* PL011 UART0 MMIO - identity-mapped so the guest stub can print.
	 * The device is owned by partition A; partition B has no UART mapping,
	 * so a B→UART access would fault at stage-2. */
	{
		.ipa_base = QEMU_UART_BASE,
		.pa_base = QEMU_UART_BASE,
		.size = 0x1000UL, /* 4 KB MMIO page */
		.attrs = HV_MEM_DEVICE_RW, /* Device nGnRE, never executable */
	},
	/* Read-only Normal memory, never executable. Partition A does not
	 * touch it; it lets read-back (A1) see write-permission drift. */
	{
		.ipa_base = PART_A_RO_IPA_BASE,
		.pa_base = PART_A_RO_PA_BASE,
		.size = PART_A_RO_SIZE,
		.attrs = HV_MEM_R,
	},
};

static const struct hv_mem_region part_b_regions[] = {
	{
		.ipa_base = PART_B_IPA_BASE,
		.pa_base = PART_B_PA_BASE,
		.size = PART_B_SIZE,
		.attrs = HV_MEM_RWX,
	},
};

static const struct hv_partition_mem part_a_mem_cfg = {
	.partition_id = PARTITION_A_ID,
	.regions = part_a_regions,
	.region_count = 3U, /* DRAM + UART MMIO + read-only region */
};

/* -----------------------------------------------------------------------
 * DMA grants
 *
 * A grant gives a partition one device: its hardware StreamID gets a DMA
 * window (device address -> physical address inside the partition, with
 * access rights), and only once the SMMU enforces that window are the
 * device's registers mapped into the partition.  If the SMMU cannot
 * enforce it (e.g. no stage 2), or the device is absent, the device is
 * withheld: its stream stays aborted and its registers stay unmapped.  The
 * partition learns which grants are in force from a bit mask in x0 at entry.
 *
 * For a PCI function Haven places the memory BAR itself and turns on bus
 * mastering only once the window is enforced; the partition gets the
 * function's config page read-only, so it cannot move the BAR or change the
 * command register (writes fault at stage 2).
 *
 * The window uses the partition's own addresses (IOVA = IPA), so a device
 * and the partition name the same buffer the same way.
 * ----------------------------------------------------------------------- */

#define GUEST_ARG_DMA0 (1UL << 0) /* x0 bit: grant 0 is in force */

struct part_dma_grant {
	hv_u32 partition_id;
	hv_u16 streamid;
	hv_u64 iova;
	hv_u64 size;
	hv_u64 pa;
	hv_dma_access_t access;
	hv_u64 cfg_pa; /* PCI config page (0: not a PCI function) */
	hv_u32 bar; /* memory BAR Haven places */
	hv_u64 bar_pa;
	hv_u64 bar_size;
	const struct hv_mem_region *mmio;
	hv_u32 mmio_count;
	hv_u64 guest_flag;
};

static const struct hv_mem_region part_a_dma_mmio[] = {
	{
		.ipa_base = PART_A_DMA_CFG_PA,
		.pa_base = PART_A_DMA_CFG_PA,
		.size = 0x1000UL, /* config space of 00:01.0 only */
		.attrs = HV_MEM_DEVICE | HV_MEM_R, /* read-only */
	},
	{
		.ipa_base = PART_A_DMA_BAR_PA,
		.pa_base = PART_A_DMA_BAR_PA,
		.size = PART_A_DMA_BAR_SIZE,
		.attrs = HV_MEM_DEVICE_RW,
	},
};

static const struct part_dma_grant dma_grants[] = {
	{
		.partition_id = PARTITION_A_ID,
		.streamid = PART_A_DMA_SID,
		.iova = PART_A_DMA_IPA,
		.size = PART_A_DMA_SIZE,
		.pa = PART_A_DMA_PA,
		.access = HV_DMA_RW,
		.cfg_pa = PART_A_DMA_CFG_PA,
		.bar = PART_A_DMA_BAR,
		.bar_pa = PART_A_DMA_BAR_PA,
		.bar_size = PART_A_DMA_BAR_SIZE,
		.mmio = part_a_dma_mmio,
		.mmio_count = 2U,
		.guest_flag = GUEST_ARG_DMA0,
	},
};

/* Apply every DMA grant; returns the x0 bits for partition_id. */
static hv_u64 apply_dma_grants(hv_u32 partition_id)
{
	extern void hv_printk(const char *fmt, ...);
	hv_u64 flags = 0U;

	for (hv_u32 i = 0U; i < sizeof(dma_grants) / sizeof(dma_grants[0]);
	     ++i) {
		const struct part_dma_grant *g = &dma_grants[i];
		struct hv_partition_mem mmio = {
			.partition_id = g->partition_id,
			.regions = g->mmio,
			.region_count = g->mmio_count,
		};
		hv_status_t rc;

		if (g->partition_id != partition_id) {
			continue;
		}
		rc = HV_OK;
#ifdef HAVEN_ARCH_ARM64
		if (g->cfg_pa != 0U) {
			if (!pci_ecam_present((uintptr_t)g->cfg_pa)) {
				hv_printk(
					"HAVEN: DMA grant SID %u -> partition "
					"%u withheld: device absent\n",
					(unsigned)g->streamid,
					(unsigned)g->partition_id);
				continue;
			}
			rc = pci_ecam_place_bar((uintptr_t)g->cfg_pa, g->bar,
						g->bar_pa, g->bar_size);
		}
#endif
		if (rc == HV_OK) {
			rc = hv_smmu_assign_streamid(g->partition_id,
						     g->streamid);
		}
		if (rc == HV_OK) {
			rc = hv_smmu_configure_dma_window(g->streamid, g->iova,
							  g->size, g->pa,
							  g->access);
			if (rc == HV_OK) {
#ifdef HAVEN_ARCH_ARM64
				/* Window enforced: now the device may master. */
				if (g->cfg_pa != 0U) {
					pci_ecam_enable((uintptr_t)g->cfg_pa);
				}
#endif
				rc = hv_stage2_add_regions(&mmio);
				if (rc != HV_OK) {
					(void)hv_smmu_revoke_dma_access(
						g->streamid);
				}
			} else {
				(void)hv_smmu_release_streamid(g->streamid);
			}
		}
		if (rc != HV_OK) {
			hv_printk("HAVEN: DMA grant SID %u -> partition %u "
				  "withheld (rc=%d)\n",
				  (unsigned)g->streamid,
				  (unsigned)g->partition_id, (int)rc);
			continue;
		}
		hv_printk("HAVEN: DMA grant SID %u -> partition %u: "
			  "IOVA 0x%lx size 0x%lx\n",
			  (unsigned)g->streamid, (unsigned)g->partition_id,
			  (unsigned long)g->iova, (unsigned long)g->size);
		flags |= g->guest_flag;
	}
	return flags;
}

static const struct hv_partition_mem part_b_mem_cfg = {
	.partition_id = PARTITION_B_ID,
	.regions = part_b_regions,
	.region_count = 1U,
};

/* -----------------------------------------------------------------------
 * IRQ routing
 *
 * QEMU virt, aarch64 (INTIDs):
 *   33 - PL011 UART0, SPI 1: partition A, routed to CPU 0
 *   27 - EL1 virtual timer, PPI: partition B, on CPU 2 only
 *
 * A PPI is private to one CPU: it is owned per (INTID, CPU) and enabled in
 * that CPU's redistributor only.  It still needs an ownership entry: with
 * HCR_EL2.IMO set the physical interrupt traps to Haven, which injects it
 * only into the partition that owns it on that CPU (earlier versions of
 * this comment said a PPI needed none; the timer then never reached B).
 *
 * Interrupt ownership is exclusive: once assigned to a partition no
 * other partition can claim the same IRQ (hv_irq_assign contract).
 * ----------------------------------------------------------------------- */

static const struct hv_irq_route part_a_irqs[] = {
	{.irq_id = 33U, .owner_partition_id = PARTITION_A_ID, .target_cpu = 0U},
};

#define PART_B_CPU 2U
#define PART_B_READY_US 100000U /* CPU 2 ready deadline after CPU_ON */
#define PART_B_VTIMER_PPI 27U

static const struct hv_irq_route part_b_irqs[] = {
	{.irq_id = PART_B_VTIMER_PPI,
	 .owner_partition_id = PARTITION_B_ID,
	 .target_cpu = PART_B_CPU},
};

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
extern int hv_arch_psci_cpu_on(uint64_t target_mpidr, uintptr_t entry_pa);
extern uint64_t hv_arch_logical_mpidr(uint32_t cpu);
#define HV_ARCH_NO_CPU 0xffffffffUL /* HV_CPU_INVALID, asm/cpu_map.h */
extern int hv_arch_cpu_wait_ready(uint32_t cpu, uint64_t timeout_us);
extern int hv_arch_cpu_report_ready(uint32_t cpu);
extern void hv_arch_set_cpu_partition(uint32_t cpu, hv_u32 partition);
extern void hv_arch_cpu_init(void);
extern void hv_arch_phys_irq_cpu_init(void);
extern uint32_t hv_arch_cpu_id(void);
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
	/* Per-CPU EL2 state first (FINDINGS B2).  A CPU started by PSCI
	 * CPU_ON enters EL2 with HCR_EL2, VTCR_EL2 and VBAR_EL2 at their reset
	 * values: in QEMU HCR_EL2.RW = 0 made the ERET below an illegal
	 * exception return, and CPU 2 looped on undefined-instruction
	 * exceptions at VBAR_EL2 = 0 without ever running partition B. */
	hv_arch_cpu_init();
	hv_arch_phys_irq_cpu_init();

	hv_arch_stage2_enable(PARTITION_B_ID, PARTITION_B_ID);
	hv_arch_set_cpu_partition(hv_arch_cpu_id(), PARTITION_B_ID);

	/* FINDINGS B3: enter B only on the primary's go; a CPU that is not
	 * B's, or that reports after the primary gave up, parks here. */
	if (cpu_id != PART_B_CPU || hv_arch_cpu_id() != PART_B_CPU ||
	    !hv_arch_cpu_report_ready(PART_B_CPU)) {
		hv_arch_set_cpu_partition(hv_arch_cpu_id(), 0U);
		for (;;)
			__asm__ volatile("wfe");
	}

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
	hv_u64 part_a_arg;
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

	/* ----- DMA grants (after both partitions own their memory) -------- */

	part_a_arg = apply_dma_grants(PARTITION_A_ID);
	(void)part_a_arg; /* used on ARM64 only */

	/* ----- IRQ ownership ---------------------------------------------- */

	for (i = 0U; i < sizeof(part_a_irqs) / sizeof(part_a_irqs[0]); ++i) {
		st = hv_irq_assign(&part_a_irqs[i]);
		if (st != HV_OK) {
			hv_panic("partition A IRQ assignment failed");
		}
	}
	hv_printk("HAVEN: partition A IRQs assigned (SPI 33 -> CPU 0)\n");

	for (i = 0U; i < sizeof(part_b_irqs) / sizeof(part_b_irqs[0]); ++i) {
		st = hv_irq_assign(&part_b_irqs[i]);
		if (st != HV_OK) {
			hv_panic("partition B IRQ assignment failed");
		}
	}
	hv_printk("HAVEN: partition B IRQs assigned (PPI 27 on CPU 2)\n");

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
	if (hv_arch_logical_mpidr(PART_B_CPU) == HV_ARCH_NO_CPU) {
		/* No MPIDR to switch on: never issue CPU_ON for an index the
		 * platform's CPU map does not contain (control L5). */
		hv_printk("HAVEN: no CPU 2 in the CPU map - partition B not "
			  "started\n");
	} else if (hv_arch_psci_cpu_on(hv_arch_logical_mpidr(PART_B_CPU),
				       (uintptr_t)secondary_entry) == 0) {
		hv_arch_secondary_start(PART_B_CPU,
					partition_b_secondary_cpu_init);
		/* PSCI success only means the CPU was switched on; partition B
		 * starts when CPU 2 reports its EL2 setup done (B3). */
		if (hv_arch_cpu_wait_ready(PART_B_CPU, PART_B_READY_US))
			hv_printk("HAVEN: CPU 2 ready, partition B started "
				  "(VMID=%u)\n",
				  PARTITION_B_ID);
		else
			hv_printk("HAVEN: CPU 2 not ready within %u ms - "
				  "partition B not started\n",
				  (unsigned)(PART_B_READY_US / 1000U));
	} else {
		hv_printk(
			"HAVEN: PSCI CPU_ON failed for CPU 2 - partition B inactive\n");
	}

	hv_printk("HAVEN: ERET -> partition A entry IPA=0x%lx\n",
		  (unsigned long)PART_A_IPA_BASE);
	hv_arch_set_cpu_partition(hv_arch_cpu_id(), PARTITION_A_ID);
	hv_arch_start_partition(
		(uintptr_t)PART_A_IPA_BASE,
		(uintptr_t)(PART_A_IPA_BASE + PART_A_SIZE - 0x1000UL),
		(uintptr_t)part_a_arg);

	/* Unreachable - ERET transfers control permanently */
#endif
}
