/* SPDX-License-Identifier: Apache-2.0 */
/*
 * No-op hv_printk stub for host test builds.
 *
 * budget.c calls hv_printk() for watchdog diagnostics.  Host unit/integration
 * test binaries have no UART implementation, so this stub satisfies the linker
 * without pulling in real driver code.
 */
#include <stdarg.h>

void hv_printk(const char *fmt, ...)
{
	(void)fmt;
}
