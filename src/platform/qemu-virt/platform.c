/* SPDX-License-Identifier: Apache-2.0 */
/* Platform support for QEMU virt board (arm64).
 *
 * Memory map (QEMU virt default, ARM64):
 *   0x00000000_09000000  PL011 UART0
 *   0x00000000_08000000  GICv3 distributor (64 KiB)
 *   0x00000000_080A0000  GICv3 redistributors (64 KiB × nr_cpus)
 *   0x00000000_08080000  SMMUv3 (ARM virt SMMU)
 *   0x00000000_40000000  DDR start (up to 256 GiB)
 *
 * Counter-timer frequency: 62500000 Hz (CNTFRQ = 0x3B9ACA0... wait, let
 * QEMU provide it via CNTFRQ_EL0; we read it rather than hardcode).
 */

#include <stdint.h>
#include <haven/platform.h>
#include "drivers/uart/uart.h"
#include "src/platform/qemu-virt/memory.h"

/* -----------------------------------------------------------------------
 * PL011 UART (QEMU virt, UART0) — register-level access via drivers/uart/pl011.c
 * ----------------------------------------------------------------------- */

static void qemu_uart_putchar(char c)
{
        uart_putchar(QEMU_UART_BASE, c);
}

/* -----------------------------------------------------------------------
 * GICv3 addresses (QEMU virt)
 * ----------------------------------------------------------------------- */

static uint64_t qemu_gic_dist_base(void)  { return 0x08000000UL; }
static uint64_t qemu_gic_redist_base(void) { return 0x080A0000UL; }

/* -----------------------------------------------------------------------
 * SMMUv3 address (QEMU virt arm-smmu-v3)
 * ----------------------------------------------------------------------- */

static uint64_t qemu_smmu_base(void) { return QEMU_SMMU_BASE; }

/* -----------------------------------------------------------------------
 * Timer frequency (read from CNTFRQ_EL0 programmed by QEMU)
 * ----------------------------------------------------------------------- */

static uint64_t qemu_timer_freq(void)
{
        uint64_t freq;
        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
        return freq;
}

static uint32_t qemu_nr_cpus(void) { return 4; }

/* -----------------------------------------------------------------------
 * Platform ops table
 * ----------------------------------------------------------------------- */

static const struct hv_platform_ops qemu_virt_ops = {
        .uart_putchar    = qemu_uart_putchar,
        .gic_dist_base   = qemu_gic_dist_base,
        .gic_redist_base = qemu_gic_redist_base,
        .smmu_base       = qemu_smmu_base,
        .timer_freq      = qemu_timer_freq,
        .nr_cpus         = qemu_nr_cpus,
        .name            = "qemu-virt-arm64",
};

const struct hv_platform_ops *platform = &qemu_virt_ops;

void platform_init(void)
{
        /*
         * Initialise PL011 at 115200 8N1.  QEMU firmware already enables the
         * UART, but calling uart_init programmes the divisors explicitly so
         * the same path works on real Juno/FVP hardware.
         * ref_clk = 24 MHz (QEMU default for PL011).
         */
        uart_init(QEMU_UART_BASE, 115200u, 24000000u);
}
