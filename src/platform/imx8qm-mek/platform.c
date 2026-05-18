/* SPDX-License-Identifier: Apache-2.0 */
/* Platform support for NXP i.MX8QM MEK (Multi-Sensor Evaluation Kit).
 *
 * SoC: i.MX8QM (Cortex-A53 × 4 + Cortex-A72 × 2 + Cortex-M4F × 2)
 * Used as the secondary validation platform for Haven (cross-checking
 * assumptions: GICv3 vs GIC400, SMMU differences from i.MX95).
 *
 * Key addresses (i.MX8QM RM, SCU domain):
 *   0x5A060000  LPUART1 (debug console via SCU)
 *   0x51A00000  GIC distributor (GIC-500, GICv3 compatible)
 *   0x51B00000  GIC redistributors (stride 0x20000 × nr_cpus)
 *   0x51400000  SMMU (ARM SMMU-500, SMR-based, differs from SMMUv3)
 *   0x80000000  LPDDR4 (4 GiB on MEK)
 *
 * Note: i.MX8QM uses ARM SMMU-500 (stream-match registers, not stream
 * table entries).  The SMMUv3 driver from Phase 2 does NOT apply here.
 * This platform uses a simplified stream-match policy stub for validation.
 */

#include <stdint.h>
#include <haven/platform.h>

/* -----------------------------------------------------------------------
 * LPUART1 (i.MX8QM — same LPUART IP as i.MX95 but different base)
 * ----------------------------------------------------------------------- */

#define IMX8QM_UART_BASE        0x5A060000UL
#define LPUART_STAT             0x14
#define LPUART_DATA             0x1C
#define LPUART_STAT_TDRE        (1U << 23)

static void imx8qm_uart_putchar(char c)
{
        volatile uint32_t *stat = (volatile uint32_t *)(IMX8QM_UART_BASE + LPUART_STAT);
        volatile uint32_t *data = (volatile uint32_t *)(IMX8QM_UART_BASE + LPUART_DATA);

        while (!(*stat & LPUART_STAT_TDRE))
                __asm__ volatile("yield");

        *data = (uint32_t)(unsigned char)c;
}

/* -----------------------------------------------------------------------
 * GICv3 base addresses (i.MX8QM — GIC-500)
 * ----------------------------------------------------------------------- */

static uint64_t imx8qm_gic_dist_base(void)   { return 0x51A00000UL; }
static uint64_t imx8qm_gic_redist_base(void) { return 0x51B00000UL; }

/* -----------------------------------------------------------------------
 * SMMU base (ARM SMMU-500 — stream-match, NOT SMMUv3 stream-table)
 * ----------------------------------------------------------------------- */

static uint64_t imx8qm_smmu_base(void) { return 0x51400000UL; }

/* -----------------------------------------------------------------------
 * Timer frequency (read from CNTFRQ_EL0)
 * ----------------------------------------------------------------------- */

static uint64_t imx8qm_timer_freq(void)
{
        uint64_t freq;
        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
        return freq;
}

/* i.MX8QM Cortex-A53 cluster: 4 cores */
static uint32_t imx8qm_nr_cpus(void) { return 4; }

/* -----------------------------------------------------------------------
 * Platform ops table
 * ----------------------------------------------------------------------- */

static const struct hv_platform_ops imx8qm_ops = {
        .uart_putchar    = imx8qm_uart_putchar,
        .gic_dist_base   = imx8qm_gic_dist_base,
        .gic_redist_base = imx8qm_gic_redist_base,
        .smmu_base       = imx8qm_smmu_base,
        .timer_freq      = imx8qm_timer_freq,
        .nr_cpus         = imx8qm_nr_cpus,
        .name            = "nxp-imx8qm-mek",
};

const struct hv_platform_ops *platform = &imx8qm_ops;

void platform_init(void)
{
        /* LPUART initialized by U-Boot. No additional setup needed. */
}
