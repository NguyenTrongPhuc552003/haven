/* SPDX-License-Identifier: Apache-2.0 */
/* Hypervisor panic handler - logs message and halts all CPUs. */

#include <stdint.h>

extern void hv_printk(const char *fmt, ...);

/* -----------------------------------------------------------------------
 * hv_panic - print msg and halt the current CPU.
 *
 * In a full implementation this would also broadcast an SGI to halt
 * secondary CPUs.  For Phase 1 the primary CPU halts and secondaries
 * will detect the frozen state on their next scheduling tick.
 * ----------------------------------------------------------------------- */
void __attribute__((noreturn)) hv_panic(const char *msg)
{
	/* Disable IRQ and FIQ on this CPU to prevent re-entry */
	__asm__ volatile("msr daifset, #0xf" : : : "memory");

	hv_printk("\n*** HAVEN PANIC ***\n");
	hv_printk("  %s\n", msg);
	hv_printk("  Halting.\n");

	/* Spin forever */
	while (1) {
		__asm__ volatile("wfi" : : : "memory");
	}
}
