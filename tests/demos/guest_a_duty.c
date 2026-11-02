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

/* Aggressor for the interference measurements, chosen at build time
 * (HAVEN_DEMO_A_LOAD: 0 none, 1 CPU loop, 2 memory stream). It runs until
 * the machine stops; partition B's latency probe starts 4 s after
 * power-on, by when partition A is here. */
#ifndef GUEST_A_LOAD
#define GUEST_A_LOAD 0
#endif
#define STREAM_BASE 0x48000000UL /* inside A's RAM, above its image */
#define STREAM_SIZE (64UL << 20) /* larger than any last-level cache */

void guest_a_load(void)
{
	uint64_t sctlr;

	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
	put("PARTITION_A: caches ");
	put((sctlr & 5U) == 5U ? "on" : "OFF");
	put(" (SCTLR_EL1 M and C)\r\n");
	if (GUEST_A_LOAD == 1) {
		put("PARTITION_A: load cpu running\r\n");
		for (;;)
			__asm__ volatile("" ::: "memory");
	}
	if (GUEST_A_LOAD == 2) {
		volatile uint64_t *p = (volatile uint64_t *)STREAM_BASE;

		put("PARTITION_A: load memory stream running\r\n");
		for (uint64_t pass = 1;; pass++)
			for (uint64_t i = 0; i < STREAM_SIZE / 8U; i += 8U)
				p[i] = p[i] + pass;
	}
}
