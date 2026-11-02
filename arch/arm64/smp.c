/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/smp.c - secondary CPU ready handshake (FINDINGS B3).
 *
 * The secondary writes HV_CPU_READY into its own hv_cpu_state[] word once
 * its EL2 setup is complete, then waits; the primary waits a bounded time
 * for that word and answers through hv_cpu_go[]: HV_CPU_GO, or HV_CPU_ABORT
 * after a timeout.  The secondary enters its partition only on GO, so a
 * CPU that comes up late never enters a partition the primary has already
 * declared failed.  Each word has one writer.
 *
 * Ordering: store-release and load-acquire (STLR, LDAR) on 32-bit words,
 * plus SEV/WFE to avoid busy polling.  With EL2's stage-1 translation off
 * these words are Device memory, which no cache holds; with a future
 * cacheable inner-shareable mapping, hardware coherency keeps the same
 * protocol valid (FINDINGS A1 records the memory contract).
 */

#include <asm/cpu_map.h>
#include <asm/sysregs.h>
#include <stdint.h>

volatile uint32_t hv_cpu_state[HV_MAX_CPUS];
volatile uint32_t hv_cpu_go[HV_MAX_CPUS];

static uint64_t counter(void)
{
	uint64_t v;

	__asm__ volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(v));
	return v;
}

int hv_arch_cpu_wait_ready(uint32_t cpu, uint64_t timeout_us)
{
	uint64_t freq = read_sysreg(cntfrq_el0);
	uint64_t ticks = (freq / 1000000U) * timeout_us;
	uint64_t start = counter();
	uint32_t answer = HV_CPU_ABORT;

	if (cpu == 0U || cpu >= HV_MAX_CPUS || cpu >= hv_cpu_count)
		return 0;

	while (counter() - start < ticks) {
		if (__atomic_load_n(&hv_cpu_state[cpu], __ATOMIC_ACQUIRE) ==
		    HV_CPU_READY) {
			answer = HV_CPU_GO;
			break;
		}
	}
	__atomic_store_n(&hv_cpu_go[cpu], answer, __ATOMIC_RELEASE);
	__asm__ volatile("dsb ish\n\tsev" ::: "memory");
	return answer == HV_CPU_GO;
}

int hv_arch_cpu_report_ready(uint32_t cpu)
{
	uint32_t answer;

	if (cpu == 0U || cpu >= HV_MAX_CPUS)
		return 0;

	__atomic_store_n(&hv_cpu_state[cpu], HV_CPU_READY, __ATOMIC_RELEASE);
	__asm__ volatile("dsb ish\n\tsev" ::: "memory");
	while ((answer = __atomic_load_n(&hv_cpu_go[cpu], __ATOMIC_ACQUIRE)) ==
	       HV_CPU_WAIT)
		__asm__ volatile("wfe" ::: "memory");
	return answer == HV_CPU_GO;
}
