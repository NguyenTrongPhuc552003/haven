/* SPDX-License-Identifier: Apache-2.0 */
/* Platform support for NXP i.MX95 Dev Kit.
 *
 * SoC: i.MX95 (Cortex-A55 × 6 + Cortex-M7 × 2)
 * Memory map references: NXP i.MX95 Reference Manual (Rev. 0, 2023)
 *
 * Key addresses (from i.MX95 RM Chapter 2 "System Memory Map"):
 *   0x44380000  LPUART1 (UART used for debug console)
 *   0x48000000  GIC distributor (GICv3)
 *   0x48040000  GIC redistributors (GICv3, stride 0x20000 × 6 CPUs)
 *   0x49800000  SMMUv3 (i.MX95 SMMU)
 *   0x80000000  LPDDR5 start (2 GiB on Dev Kit)
 *
 * LPUART1 register offsets (i.MX LPUART, not PL011):
 *   0x18  STAT (Status Register)
 *   0x1C  DATA (Data Register)
 */

#include "drivers/uart/uart.h"
#include "memory.h"
#include <haven/platform.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * i.MX LPUART1 - register-level access via drivers/uart/imx_uart.c
 * ----------------------------------------------------------------------- */

static void imx95_uart_putchar(char c)
{
	uart_putchar(IMX95_UART1_BASE, c);
}

/* -----------------------------------------------------------------------
 * GICv3 base addresses (i.MX95)
 * ----------------------------------------------------------------------- */

static uint64_t imx95_gic_dist_base(void)
{
	return IMX95_GIC_DIST_BASE;
}
static uint64_t imx95_gic_redist_base(void)
{
	return IMX95_GIC_REDIST_BASE;
}

/* -----------------------------------------------------------------------
 * SMMUv3 base address (i.MX95)
 * ----------------------------------------------------------------------- */

static uint64_t imx95_smmu_base(void)
{
	return IMX95_SMMU_BASE;
}

/* -----------------------------------------------------------------------
 * Timer frequency
 * IMX95_TIMER_FREQ matches the 24 MHz crystal oscillator programmed into
 * CNTFRQ_EL0 by ATF.  Using the constant avoids an EL2 system register read
 * during early init before the MMU is live.
 * ----------------------------------------------------------------------- */

static uint64_t imx95_timer_freq(void)
{
	return IMX95_TIMER_FREQ;
}

/* i.MX95: 6 Cortex-A55 cores (0–5); Cortex-M7 runs separately */
static uint32_t imx95_nr_cpus(void)
{
	return IMX95_NR_CPUS;
}

/* -----------------------------------------------------------------------
 * Platform ops table
 * ----------------------------------------------------------------------- */

static const struct hv_platform_ops imx95_ops = {
	.uart_putchar = imx95_uart_putchar,
	.gic_dist_base = imx95_gic_dist_base,
	.gic_redist_base = imx95_gic_redist_base,
	.smmu_base = imx95_smmu_base,
	.timer_freq = imx95_timer_freq,
	.nr_cpus = imx95_nr_cpus,
	.name = "nxp-imx95-devkit",
};

const struct hv_platform_ops *platform = &imx95_ops;

void platform_init(void)
{
	/*
   * Initialise LPUART1 at 115200 8N1.
   * U-Boot leaves the UART enabled, but re-initialising is safe and
   * ensures consistent baud rate regardless of boot-loader settings.
   * ref_clk = IMX95_TIMER_FREQ (24 MHz crystal oscillator).
   */
	uart_init(IMX95_UART1_BASE, 115200u, IMX95_TIMER_FREQ);
}
