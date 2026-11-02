/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_a_duty.c - partition A reports how much it runs.
 *
 * Partition A's budget is 8 ms per 10 ms on CPU 0 (src/core/partition.c),
 * enforced by Haven's EL2 timer. Over a 200 ms window it should run about
 * 80% of the time, with gaps of about 2 ms.
 */

#include "guest_duty.h"
#include <stdint.h>

#define UART_BASE 0x09000000UL
#define UART_FR (UART_BASE + 0x18UL)

static void put(const char *s)
{
	while (*s) {
		while (*(volatile uint32_t *)UART_FR & (1U << 5))
			;
		*(volatile uint32_t *)UART_BASE = (uint32_t)(unsigned char)*s++;
	}
}

static void put_u32(uint32_t v)
{
	char b[11];
	int i = 10;

	b[i] = '\0';
	do {
		b[--i] = (char)('0' + v % 10U);
		v /= 10U;
	} while (v != 0U && i > 0);
	put(&b[i]);
}

void guest_a_duty_test(void)
{
	struct guest_duty d;

	guest_duty_measure(&d, 200U, 200U);
	put("PARTITION_A: duty ");
	put_u32(d.permille);
	put(" permille, longest gap ");
	put_u32(d.max_gap_us);
	put(" us, gaps ");
	put_u32(d.gaps);
	put("\r\nPARTITION_A: duty test done\r\n");
}
