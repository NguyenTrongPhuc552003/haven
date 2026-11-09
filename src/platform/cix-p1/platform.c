/* SPDX-License-Identifier: Apache-2.0 */
/* Platform support for CIX P1 (Sky1): Orange Pi 6 Plus, DRAFT (D4).
 *
 * Compile-only until a board exists: built and host-tested, never run.
 * Sources and assumptions: memory.h and docs/methodology/stage2/
 * 20261109-cix-p1-platform/README.md. HAVEN at EL2 on 12 cores, console on
 * UART3, GIC-700 found by GICR_TYPER affinity, PSCI to firmware over SMC
 * (sky1.dtsi /psci method "smc"; CMake selects the conduit).
 */

#include "drivers/uart/uart.h"
#include "src/platform/cix-p1/memory.h"
#include <haven/platform.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Logical CPU map (asm/cpu_map.h), boot CPU first: MADT order of the CIX
 * BIOS (Madt.aslc, GET_MPID(core, 0) = core << 8), core 4 first (ledger
 * "CPU numbering"). Logical 0 to 7 (cores 4, 0 to 3, 5 to 7) are
 * partition A's, 8 to 11 (cores 8 to 11) partition B's (owner, D3).
 * ----------------------------------------------------------------------- */
const uint64_t hv_cpu_mpidr[] = {0x400, 0x000, 0x100, 0x200, 0x300, 0x500,
				 0x600, 0x700, 0x800, 0x900, 0xa00, 0xb00};
const uint32_t hv_cpu_count = CIX_NR_CPUS;
_Static_assert(sizeof(hv_cpu_mpidr) / sizeof(hv_cpu_mpidr[0]) == CIX_NR_CPUS,
	       "one MPIDR per CPU");

/* -----------------------------------------------------------------------
 * Haven's EL2 map (asm/el2_mmu.h): only what Haven itself touches.
 *   0x00_0000_0000 + 1 GiB  Device: UART3, SMMU0, GIC-700 (all below 1 GiB)
 *   Haven's image            Normal: HAVEN_LOAD_PA + HAVEN_SIZE (see memory.h)
 * Partition RAM is deliberately absent.
 * ----------------------------------------------------------------------- */
#include <asm/el2_mmu.h>
const struct hv_el2_region hv_el2_regions[] = {
	{0x0000000000UL, 0x40000000UL, HV_EL2_DEVICE},
	{HAVEN_LOAD_PA, HAVEN_SIZE, HV_EL2_NORMAL},
};
const uint32_t hv_el2_region_count =
	sizeof(hv_el2_regions) / sizeof(hv_el2_regions[0]);

static void cix_uart_putchar(char c)
{
	uart_putchar(CIX_UART3_BASE, c);
}

static uint64_t cix_gic_dist_base(void)
{
	return CIX_GIC_DIST_BASE;
}

static uint64_t cix_gic_redist_base(void)
{
	return CIX_GIC_REDIST_BASE;
}

static uint64_t cix_smmu_base(void)
{
	return CIX_SMMU0_BASE;
}

static uint32_t cix_smmu_evtq_irq(void)
{
	return CIX_SMMU0_EVTQ_INTID;
}

/* Counter frequency as EL3 firmware programmed CNTFRQ_EL0 (the CIX BIOS
 * sets PcdArmArchTimerFreqInHz = 1 GHz for boards; read, not assumed). */
static uint64_t cix_timer_freq(void)
{
	uint64_t freq;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	return freq;
}

static uint32_t cix_nr_cpus(void)
{
	return CIX_NR_CPUS;
}

static const struct hv_platform_ops cix_p1_ops = {
	.uart_putchar = cix_uart_putchar,
	.gic_dist_base = cix_gic_dist_base,
	.gic_redist_base = cix_gic_redist_base,
	.smmu_base = cix_smmu_base,
	.smmu_evtq_irq = cix_smmu_evtq_irq,
	.timer_freq = cix_timer_freq,
	.nr_cpus = cix_nr_cpus,
	.name = "cix-p1-orangepi-6-plus (draft)",
};

const struct hv_platform_ops *platform = &cix_p1_ops;

void platform_init(void)
{
	/* UART3 at 115200 8N1 from the 100 MHz reference (memory.h,
	 * ASSUMPTION for UART3). Its pins come from the vendor's "40pin"
	 * BIOS variant: HAVEN has no pinmux code. */
	uart_init(CIX_UART3_BASE, CIX_UART_BAUD, CIX_UART_CLK_HZ);
}
