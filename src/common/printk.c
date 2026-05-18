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
 * All output goes through platform_uart_putchar() provided by
 * src/platform/<board>/platform.c.  No buffering; each character
 * is written immediately (early boot safe).
 */

#include <haven/platform.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

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
}
