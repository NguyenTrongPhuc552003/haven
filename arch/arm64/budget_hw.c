/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/budget_hw.c - CPU time budgets enforced with the EL2 timer.
 *
 * Each CPU runs one partition (static partitioning). Its budget is
 * budget_ns in every period_ns. When a period starts Haven arms the EL2
 * physical timer (CNTHP, PPI 26) at period start + budget; when it fires,
 * Haven keeps the CPU at EL2 until the next period starts, then re-arms
 * and returns to the partition. A partition that uses less than its budget
 * is never interrupted: there is no other partition on that CPU to give
 * the time to, so throttling only bounds what this partition may consume
 * (the contract the interference study needs).
 *
 * The wait is a busy loop on the physical counter with interrupts masked
 * at EL2: the partition's own interrupts stay pending until it resumes;
 * Haven's own interrupts on that CPU (the SMMU event queue on CPU 0) wait
 * up to period - budget. Recorded as a limit.
 *
 * Timer registers: Arm ARM D24 (CNTHP_CTL_EL2 ENABLE bit 0, IMASK bit 1).
 */

#include <asm/cpu_map.h>
#include <asm/gic.h>
#include <asm/sysregs.h>
#include <haven/types.h>
#include <stdint.h>

#include "drivers/irqchip/gic_v3.h"

/* Layout read by scripts/qemu/partition-b-check.py: five 64-bit words,
 * then four 32-bit words (56 bytes per CPU). */
struct cpu_budget {
	uint64_t period; /* ticks */
	uint64_t budget; /* ticks */
	uint64_t start; /* counter value at the current period start */
	uint64_t throttled; /* ticks spent held at EL2 */
	uint64_t max_late; /* ticks between the deadline and Haven seeing it */
	uint32_t active;
	uint32_t throttles;
	uint32_t missed; /* expiries seen after the period had already ended */
	uint32_t pad;
};

/* Read by tools from Haven's image; written only by its own CPU. */
struct cpu_budget hv_cpu_budget[HV_MAX_CPUS];

static uint64_t counter(void)
{
	uint64_t v;

	__asm__ volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(v));
	return v;
}

static uint64_t ns_to_ticks(uint64_t ns)
{
	uint64_t freq = read_sysreg(cntfrq_el0);

	return (ns / 1000U) * (freq / 1000000U) +
	       ((ns % 1000U) * (freq / 1000000U)) / 1000U;
}

int hv_arch_budget_start(uint32_t cpu, uint64_t budget_ns, uint64_t period_ns)
{
	struct cpu_budget *b;

	if (cpu >= HV_MAX_CPUS || budget_ns == 0U || budget_ns >= period_ns)
		return 0; /* nothing to enforce */

	b = &hv_cpu_budget[cpu];
	b->period = ns_to_ticks(period_ns);
	b->budget = ns_to_ticks(budget_ns);
	b->throttles = 0U;
	b->throttled = 0U;
	b->max_late = 0U;
	b->missed = 0U;
	if (gic_v3_set_ppi_priority(cpu, HV_CNTHP_PPI, GIC_PRIO_HYPERVISOR) !=
		    HV_OK ||
	    gic_v3_enable_ppi(cpu, HV_CNTHP_PPI) != HV_OK)
		return 0;
	b->start = counter();
	write_sysreg(b->start + b->budget, cnthp_cval_el2);
	write_sysreg(CNTHP_CTL_ENABLE, cnthp_ctl_el2);
	__asm__ volatile("isb" ::: "memory");
	b->active = 1U;
	return 1;
}

/* EL2 timer interrupt on cpu: the budget of the current period is spent.
 * Returns 1 if it was a budget expiry (the caller then completes the
 * interrupt), 0 if this CPU enforces no budget. */
int hv_arch_budget_expired(uint32_t cpu)
{
	struct cpu_budget *b;
	uint64_t now;
	uint64_t next;

	/* The level-sensitive timer output drops once IMASK is set. */
	write_sysreg(CNTHP_CTL_ENABLE | CNTHP_CTL_IMASK, cnthp_ctl_el2);
	__asm__ volatile("isb" ::: "memory");
	if (cpu >= HV_MAX_CPUS || !hv_cpu_budget[cpu].active)
		return 0;

	b = &hv_cpu_budget[cpu];
	now = counter();
	if (now - (b->start + b->budget) > b->max_late)
		b->max_late = now - (b->start + b->budget);
	/* Conservative on a late expiry: hold until the next period boundary
	 * on the fixed grid, even if that costs the partition most of a
	 * period. It never lets a partition run more than its budget plus the
	 * delivery delay in any period; it can serve it less (observed in
	 * QEMU, where the expiry arrives 0.5 to 2 ms late). */
	next = b->start + b->period;
	if (next <= now)
		b->missed++;
	while (next <= now)
		next += b->period;
	b->throttles++;
	while (counter() < next)
		__asm__ volatile("yield");
	b->throttled += next - now;
	b->start = next;
	write_sysreg(b->start + b->budget, cnthp_cval_el2);
	write_sysreg(CNTHP_CTL_ENABLE, cnthp_ctl_el2);
	__asm__ volatile("isb" ::: "memory");
	return 1;
}

uint32_t hv_arch_budget_throttles(uint32_t cpu)
{
	return cpu < HV_MAX_CPUS ? hv_cpu_budget[cpu].throttles : 0U;
}
