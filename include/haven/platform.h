/* SPDX-License-Identifier: Apache-2.0 */
/* Common platform interface for the Haven hypervisor.
 *
 * Each board provides an implementation of hv_platform_ops and assigns
 * it to the global `platform` pointer in platform_init().
 * The rest of the hypervisor calls only through this interface. */

#ifndef HAVEN_PLATFORM_H
#define HAVEN_PLATFORM_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Platform operations table
 * ----------------------------------------------------------------------- */

struct hv_platform_ops {
	/* Early UART output (pre-driver, polled) */
	void (*uart_putchar)(char c);

	/* Base physical addresses of hardware blocks */
	uint64_t (*gic_dist_base)(void); /* GIC Distributor MMIO base */
	uint64_t (*gic_redist_base)(void); /* GIC Redistributor MMIO base */
	uint64_t (*smmu_base)(void); /* SMMUv3 MMIO base */

	/* CPU / timer parameters */
	uint64_t (*timer_freq)(void); /* Counter-timer frequency (Hz) */
	uint32_t (*nr_cpus)(void); /* Number of CPUs in this cluster */

	/* Platform name string (for diagnostics) */
	const char *name;
};

/* Global platform pointer - set by platform_init() */
extern const struct hv_platform_ops *platform;

/* Called from haven_init() to initialize board-specific hardware */
void platform_init(void);

/* Convenience wrapper: output a character via platform UART */
static inline void platform_uart_putchar(char c)
{
	if (platform && platform->uart_putchar)
		platform->uart_putchar(c);
}

#endif /* HAVEN_PLATFORM_H */
