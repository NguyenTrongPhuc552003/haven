/* SPDX-License-Identifier: Apache-2.0 */
/* Haven hypervisor main initialization sequence.
 *
 * haven_init() is called once by the primary CPU after the boot stub
 * (arch/arm64/boot.S) has zeroed BSS and set up the stack.
 *
 * Order of initialization matters:
 *   1. Architecture layer (exception vectors, CPU registers)
 *   2. Platform layer (UART for early output, timer frequency)
 *   3. Isolation policy modules (stage2, irq, budget, smmu, iommu, timer, el2)
 *   4. Partition launch
 */

#include <haven/budget_sched.h>
#include <haven/el2_exceptions.h>
#include <haven/iommu.h>
#include <haven/irq_ownership.h>
#include <haven/platform.h>
#include <haven/smmu.h>
#include <haven/stage2.h>
#include <haven/timer.h>
#include <haven/types.h>
#include <stdint.h>

#ifdef HAVEN_ARCH_ARM64
#include "drivers/iommu/smmu_v3.h"
#include "drivers/irqchip/gic_v3.h"
#endif

/* Forward declarations for platform and arch layers */
extern void platform_init(void);
extern void hv_arch_cpu_init(void);
extern void hv_arch_timer_init(void);
extern void partitions_launch(void);

/* Forward declarations defined later in this file */
static void hv_print_banner(void);
static void hv_check_hardware_features(void);

/* -----------------------------------------------------------------------
 * haven_init - hypervisor entry point (called from boot.S)
 *
 * dtb_pa: physical address of the device tree blob passed by firmware.
 *         Used to discover memory map and partition configuration.
 * ----------------------------------------------------------------------- */
void haven_init(uintptr_t dtb_pa)
{
	/* Step 1: ARM64 architecture layer
   * Programs HCR_EL2, VTCR_EL2, SCTLR_EL2, VTTBR_EL2=0,
   * installs exception vectors, enables GICv3 system registers. */
	hv_arch_cpu_init();

	/* Step 2: Platform layer
   * Initializes early UART (for hv_printk), reads GIC/SMMU base
   * addresses, and programs the generic timer frequency. */
	platform_init();

	/* Print banner now that UART is available */
	hv_print_banner();

	/* Step 3: Verify hardware supports all required features */
	hv_check_hardware_features();

	/* Step 4: EL2 physical timer
   * Enables CNTHP_CTL_EL2 with IMASK set (no interrupt until deadline). */
	hv_arch_timer_init();

#ifdef HAVEN_ARCH_ARM64
	if (gic_v3_init((uintptr_t)platform->gic_dist_base(),
			(uintptr_t)platform->gic_redist_base(),
			platform->nr_cpus()) != HV_OK) {
		extern void hv_panic(const char *msg);
		hv_panic("GICv3 initialization failed");
	}

	if (smmu_v3_init((uintptr_t)platform->smmu_base(), 256U) != HV_OK) {
		extern void hv_panic(const char *msg);
		hv_panic("SMMUv3 initialization failed");
	}
#endif

	/* Step 5: Isolation policy modules
   * These are the core haven subsystems.  All operate on static state
   * (no heap).  Initialization order does not matter between them,
   * but all must complete before partitions are launched. */
	hv_stage2_init();
	hv_irq_owner_init();
	hv_budget_sched_init();
	hv_smmu_init();
	hv_iommu_init();
	hv_timer_init();
	hv_el2_exceptions_init();

	/* Step 6: Load partition configuration from DTB or built-in table.
   * For Phase 1, a minimal built-in config is used (no DTB parsing). */
	(void)dtb_pa; /* DTB parsing added in Phase 2/3 */

	/* Step 7: Launch partitions
   * Programs stage-2 tables, assigns IRQs and budgets, then
   * performs ERET to each partition's entry point. */
	partitions_launch();

	/* Idle loop - primary CPU waits for timer/IRQ events */
	while (1) {
		__asm__ volatile("wfi" : : : "memory");
	}
}

/* -----------------------------------------------------------------------
 * hv_print_banner - log hypervisor version to early UART.
 * ----------------------------------------------------------------------- */
static void hv_print_banner(void)
{
	/* hv_printk is defined in src/common/printk.c */
	extern void hv_printk(const char *fmt, ...);
	hv_printk("Haven hypervisor starting\n");
	hv_printk("  Version: 0.6.2\n");
	hv_printk("  Build:   " __DATE__ " " __TIME__ "\n");
}

/* -----------------------------------------------------------------------
 * hv_check_hardware_features - verify required ARM64 features are present.
 *
 * Reads ID_AA64MMFR0_EL1 to confirm stage-2 page tables are supported.
 * Reads ID_AA64PFR0_EL1 to confirm GICv3 system registers are available.
 * Halts with a fatal message if any required feature is absent.
 * ----------------------------------------------------------------------- */
static void hv_check_hardware_features(void)
{
	extern void hv_printk(const char *fmt, ...);
	extern void hv_panic(const char *msg);

	/* ID_AA64MMFR0_EL1 bits [3:0]: VMSAv8-64 stage-2 support.
   * Value >= 1 means stage-2 is supported. */
	uint64_t mmfr0 = 0;
	__asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
	uint32_t parange = (uint32_t)(mmfr0 & 0xf);
	if (parange == 0) {
		hv_panic("CPU does not support stage-2 MMU");
	}

	/* ID_AA64PFR0_EL1 bits [27:24]: GIC CPU interface version.
   * 0b0001 = GICv3/v4 system registers supported. */
	uint64_t pfr0 = 0;
	__asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r"(pfr0));
	uint32_t gic = (uint32_t)((pfr0 >> 24) & 0xf);
	if (gic == 0) {
		hv_panic("CPU does not support GICv3 system registers");
	}

	hv_printk("  CPU: PARange=%u GIC=v3 - features OK\n", parange);
}
