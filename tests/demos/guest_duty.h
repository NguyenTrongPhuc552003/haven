/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_duty.h - how much of the wall-clock time a guest runs.
 *
 * The guest spins on the virtual counter (CNTVCT_EL0, which keeps counting
 * while the guest is held at EL2) for a fixed window. A step between two
 * consecutive reads longer than the threshold is time the guest did not
 * run; everything else is running time. With a budget of B per period P
 * the expected duty is B / P and the longest gap about P - B.
 */
#ifndef GUEST_DUTY_H
#define GUEST_DUTY_H

#include <stdint.h>

struct guest_duty {
	uint32_t permille; /* running time / window, in 1/1000 */
	uint32_t max_gap_us;
	uint32_t gaps;
};

static inline uint64_t guest_cntvct(void)
{
	uint64_t v;

	__asm__ volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(v));
	return v;
}

/* window_ms of wall time; gaps longer than gap_us are not running time. */
static inline void guest_duty_measure(struct guest_duty *d, uint32_t window_ms,
				      uint32_t gap_us)
{
	uint64_t freq;
	uint64_t t0, last, now, step, ran = 0, max_gap = 0, window, thr;
	uint32_t gaps = 0;

	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	window = (freq / 1000U) * window_ms;
	thr = (freq / 1000000U) * gap_us;
	t0 = last = guest_cntvct();
	do {
		now = guest_cntvct();
		step = now - last;
		if (step > thr) {
			gaps++;
			if (step > max_gap)
				max_gap = step;
		} else {
			ran += step;
		}
		last = now;
	} while (now - t0 < window);
	d->permille = (uint32_t)((ran * 1000U) / (now - t0));
	d->max_gap_us = (uint32_t)(max_gap / (freq / 1000000U));
	d->gaps = gaps;
}

#endif
