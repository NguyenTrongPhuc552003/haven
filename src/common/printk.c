/* SPDX-License-Identifier: Apache-2.0 */
/* Hypervisor formatted output (hv_printk).
 *
 * Implements a minimal subset of printf:
 *   %s  string
 *   %c  character
 *   %u  unsigned decimal (uint32_t)
 *   %lu unsigned decimal (uint64_t)
 *   %x  unsigned hex (uint32_t, lowercase, no leading zeros)
 *   %lx unsigned hex (uint64_t, lowercase, no leading zeros)
 *   %p  pointer (64-bit hex with 0x prefix)
 *   %%  literal percent
 *
 * Lines from several CPUs (multi-vCPU partitions) are kept whole by a
 * line lock: a CPU holds it from its first character until it has
 * printed a format ending in a newline, so a line built from several
 * calls stays whole. A CPU that cannot get it within 50 ms prints anyway
 * (a CPU parked in the middle of a line costs interleaving, not a hang).
 * The lock is used
 * only once EL2 stage-1 translation is on (hv_el2_ttbr): before, HAVEN's
 * memory is Device memory, where exclusives need not work, and only the
 * boot CPU runs. docs/methodology/stage2/20261108-printk-lock/.
 *
 * All output goes through platform_uart_putchar() provided by
 * src/platform/<board>/platform.c.  No buffering; each character
 * is written immediately (early boot safe).
 */

#include <haven/platform.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern volatile uint64_t hv_el2_ttbr;
extern uint32_t hv_arch_cpu_id(void);

#define PRINT_WAIT_MS 50U

static uint32_t print_owner; /* 0: free, else CPU index + 1 */

static uint64_t print_counter(void)
{
	uint64_t v;

	__asm__ volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(v));
	return v;
}

/* Take the line lock for the calling CPU; 1 if it holds it afterwards. */
static int print_lock(void)
{
	uint32_t me;
	uint64_t freq, start;

	if (hv_el2_ttbr == 0U)
		return 0;
	me = hv_arch_cpu_id() + 1U; /* 0 for a CPU not in the map */
	if (me == 0U)
		return 0;
	if (__atomic_load_n(&print_owner, __ATOMIC_RELAXED) == me)
		return 1; /* line in progress, or nested */
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	start = print_counter();
	do {
		uint32_t free = 0U;

		if (__atomic_compare_exchange_n(&print_owner, &free, me, 0,
						__ATOMIC_ACQUIRE,
						__ATOMIC_RELAXED))
			return 1;
	} while (print_counter() - start < (freq / 1000U) * PRINT_WAIT_MS);
	return 0;
}

static void putstr(const char *s)
{
	if (!s)
		s = "(null)";
	while (*s)
		platform_uart_putchar(*s++);
}

static void puthex(uint64_t val, int width)
{
	static const char hex[] = "0123456789abcdef";
	char buf[17];
	int i = 0;

	if (val == 0) {
		buf[i++] = '0';
	} else {
		uint64_t v = val;
		while (v && i < 16) {
			buf[i++] = hex[v & 0xf];
			v >>= 4;
		}
	}

	/* Pad with leading zeros if width specified */
	while (i < width)
		buf[i++] = '0';

	/* Print in reverse */
	while (i > 0)
		platform_uart_putchar(buf[--i]);
}

static void putdec(uint64_t val)
{
	char buf[21];
	int i = 0;

	if (val == 0) {
		platform_uart_putchar('0');
		return;
	}
	while (val && i < 20) {
		buf[i++] = (char)('0' + (val % 10));
		val /= 10;
	}
	while (i > 0)
		platform_uart_putchar(buf[--i]);
}

/* -----------------------------------------------------------------------
 * hv_printk - formatted output to hypervisor UART.
 * ----------------------------------------------------------------------- */
void hv_printk(const char *fmt, ...)
{
	va_list ap;
	int locked = print_lock();
	const char *fmt0 = fmt;
	const char *end = fmt;

	while (*end)
		end++;
	va_start(ap, fmt);

	while (*fmt) {
		if (*fmt != '%') {
			platform_uart_putchar(*fmt++);
			continue;
		}
		fmt++; /* skip '%' */

		int is_long = 0;
		if (*fmt == 'l') {
			is_long = 1;
			fmt++;
		}

		switch (*fmt) {
		case 's': {
			const char *s = va_arg(ap, const char *);
			putstr(s);
			break;
		}
		case 'c': {
			int c = va_arg(ap, int);
			platform_uart_putchar((char)c);
			break;
		}
		case 'u': {
			uint64_t v = is_long ?
					     va_arg(ap, uint64_t) :
					     (uint64_t)va_arg(ap, unsigned int);
			putdec(v);
			break;
		}
		case 'd': {
			int64_t v = is_long ? (int64_t)va_arg(ap, long) :
					      (int64_t)va_arg(ap, int);
			if (v < 0) {
				platform_uart_putchar('-');
				putdec((uint64_t)(-v));
			} else {
				putdec((uint64_t)v);
			}
			break;
		}
		case 'x': {
			uint64_t v = is_long ?
					     va_arg(ap, uint64_t) :
					     (uint64_t)va_arg(ap, unsigned int);
			puthex(v, 0);
			break;
		}
		case 'p': {
			uintptr_t v = (uintptr_t)va_arg(ap, void *);
			putstr("0x");
			puthex((uint64_t)v, 16);
			break;
		}
		case '%': {
			platform_uart_putchar('%');
			break;
		}
		default: {
			platform_uart_putchar('%');
			if (is_long)
				platform_uart_putchar('l');
			platform_uart_putchar(*fmt);
			break;
		}
		}
		fmt++;
	}

	va_end(ap);
	/* A line ends with a format that ends in a newline: the lock is
	 * released only then (no state shared between CPUs decides it). */
	if (locked && end != fmt0 && end[-1] == '\n')
		__atomic_store_n(&print_owner, 0U, __ATOMIC_RELEASE);
}
