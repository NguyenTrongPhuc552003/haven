/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_b_timer.c - partition B status record and timer test.
 *
 * Partition B has no UART, so it reports through a status record in its own
 * RAM (guest_b_status).  The host reads the record over the QEMU monitor
 * (scripts/qemu/partition-b-check.py): the symbol's address from guest_b.elf
 * is an IPA, and partition B's stage-2 maps IPA 0x60000000 to PA 0xA0800000.
 *
 * The timer test uses the EL1 virtual timer (CNTV, PPI 27).  Haven owns the
 * physical PPI on CPU 2 for partition B and injects it linked to the
 * physical interrupt; the handler reprograms or stops the timer before EOI,
 * so the level-triggered line drops before the physical interrupt is
 * deactivated by the guest's EOI.
 *
 * Timer registers: Arm ARM D24 (CNTV_CTL_EL0 ENABLE bit 0, IMASK bit 1).
 */

#include "guest_duty.h"
#include <stdint.h>

#define GUEST_B_MAGIC 0x48564e42U /* "HVNB" */
#define VTIMER_INTID 27U
#define TIMER_IRQS 8U
#define POLL_LIMIT 200000000U
#define PROBE_IRQS 500U

enum {
	RESULT_NONE = 0U,
	RESULT_PASS = 1U,
	RESULT_TOO_FEW = 2U,
	RESULT_OTHER_IRQ = 3U,
};

/* Layout read by partition-b-check.py: 19 32-bit words. */
struct guest_b_status {
	uint32_t magic;
	uint32_t stage; /* 1 started, 2 periods, 3 resumed, 4 timer, 5 done */
	uint32_t timer_irqs;
	uint32_t other_irqs;
	uint32_t last_intid;
	uint32_t result;
	uint32_t duty_permille; /* running time over a 200 ms window */
	uint32_t max_gap_us; /* longest time not running in that window */
	uint32_t gaps;
	/* Victim probe (interference plan, prerequisite 2): delay from the
	 * programmed virtual-timer deadline to this handler. */
	uint32_t lat_samples;
	uint32_t lat_max_ns;
	uint32_t lat_avg_ns;
	uint32_t lat_hist[5]; /* < 10 us, < 100 us, < 1 ms, < 10 ms, more */
	uint32_t sctlr_el1; /* M bit 0, C bit 2, I bit 12: caches on? */
	uint32_t pad;
};

volatile struct guest_b_status guest_b_status
	__attribute__((section(".data"), used)) = {0};

static uint64_t period;
static uint32_t probe_left; /* > 0: latency probe running */
static uint64_t lat_sum_ticks;
static uint64_t freq_hz;

static void probe_sample(void)
{
	uint64_t now, cval, d, ns;

	__asm__ volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(now));
	__asm__ volatile("mrs %0, cntv_cval_el0" : "=r"(cval));
	d = now > cval ? now - cval : 0U;
	ns = d * 1000000000ULL / freq_hz;
	lat_sum_ticks += d;
	guest_b_status.lat_samples++;
	if (ns > guest_b_status.lat_max_ns)
		guest_b_status.lat_max_ns =
			(uint32_t)(ns > 0xffffffffULL ? 0xffffffffULL : ns);
	guest_b_status.lat_hist[ns < 10000U    ? 0 :
				ns < 100000U   ? 1 :
				ns < 1000000U  ? 2 :
				ns < 10000000U ? 3 :
						 4]++;
}

void guest_b_stage(uint32_t stage)
{
	guest_b_status.magic = GUEST_B_MAGIC;
	guest_b_status.stage = stage;
}

void guest_irq_handler(void)
{
	uint64_t iar;
	uint32_t intid;

	__asm__ volatile("mrs %0, icc_iar1_el1" : "=r"(iar));
	intid = (uint32_t)(iar & 0xffffffU);
	if (intid == VTIMER_INTID && probe_left != 0U) {
		probe_sample();
		if (--probe_left != 0U)
			__asm__ volatile("msr cntv_tval_el0, %0" ::"r"(period));
		else
			__asm__ volatile("msr cntv_ctl_el0, %0" ::"r"(0UL));
		__asm__ volatile("isb");
	} else if (intid == VTIMER_INTID) {
		guest_b_status.timer_irqs++;
		if (guest_b_status.timer_irqs < TIMER_IRQS)
			__asm__ volatile("msr cntv_tval_el0, %0" ::"r"(period));
		else
			__asm__ volatile("msr cntv_ctl_el0, %0" ::"r"(0UL));
		__asm__ volatile("isb");
	} else if (intid < 1020U) {
		guest_b_status.other_irqs++;
	}
	if (intid < 1020U)
		guest_b_status.last_intid = intid;
	__asm__ volatile("msr icc_eoir1_el1, %0\n\tisb" ::"r"(iar));
}

void guest_b_timer_test(void)
{
	extern char guest_vectors[];
	uint64_t sre;
	uint64_t freq;

	__asm__ volatile("msr vbar_el1, %0\n\tisb" ::"r"(guest_vectors));
	__asm__ volatile("mrs %0, icc_sre_el1" : "=r"(sre));
	__asm__ volatile("msr icc_sre_el1, %0\n\tisb" ::"r"(sre | 1U));
	__asm__ volatile("msr icc_pmr_el1, %0" ::"r"(0xffUL));
	__asm__ volatile("msr icc_igrpen1_el1, %0\n\tisb" ::"r"(1UL));

	/* 1 ms period. */
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	period = freq / 1000U;
	if (period == 0U)
		period = 1U;

	__asm__ volatile("msr cntv_tval_el0, %0" ::"r"(period));
	__asm__ volatile("msr cntv_ctl_el0, %0\n\tisb" ::"r"(1UL));
	guest_b_stage(4U);

	__asm__ volatile("msr daifclr, #2" ::: "memory");
	for (uint32_t i = 0;
	     i < POLL_LIMIT && guest_b_status.timer_irqs < TIMER_IRQS; i++)
		;
	__asm__ volatile("msr daifset, #2" ::: "memory");
	__asm__ volatile("msr cntv_ctl_el0, %0\n\tisb" ::"r"(0UL));

	if (guest_b_status.timer_irqs < TIMER_IRQS)
		guest_b_status.result = RESULT_TOO_FEW;
	else if (guest_b_status.other_irqs != 0U)
		guest_b_status.result = RESULT_OTHER_IRQ;
	else
		guest_b_status.result = RESULT_PASS;

	/* Budget: 2 ms per 10 ms on CPU 2 (src/core/partition.c). */
	{
		struct guest_duty d;

		guest_duty_measure(&d, 200U, 200U);
		guest_b_status.duty_permille = d.permille;
		guest_b_status.max_gap_us = d.max_gap_us;
		guest_b_status.gaps = d.gaps;
	}

	/* Latency probe: start once partition A is past its own tests (4 s
	 * after power-on on the virtual counter), then 500 interrupts 1 ms
	 * apart. In QEMU the numbers are the host's, not the board's. */
	{
		uint64_t now, sctlr;

		__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
		guest_b_status.sctlr_el1 = (uint32_t)sctlr;
		freq_hz = freq;
		do
			__asm__ volatile("isb\n\tmrs %0, cntvct_el0"
					 : "=r"(now));
		while (now < freq * 4U);
		probe_left = PROBE_IRQS;
		__asm__ volatile("msr cntv_tval_el0, %0" ::"r"(period));
		__asm__ volatile("msr cntv_ctl_el0, %0\n\tisb" ::"r"(1UL));
		__asm__ volatile("msr daifclr, #2" ::: "memory");
		while (probe_left != 0U)
			__asm__ volatile("wfi");
		__asm__ volatile("msr daifset, #2" ::: "memory");
		if (guest_b_status.lat_samples != 0U)
			guest_b_status.lat_avg_ns =
				(uint32_t)(lat_sum_ticks * 1000000000ULL /
					   freq / guest_b_status.lat_samples);
	}
	guest_b_stage(5U);
}
