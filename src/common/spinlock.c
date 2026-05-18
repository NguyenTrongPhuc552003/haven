/* SPDX-License-Identifier: Apache-2.0 */
/* ARMv8 spinlock implementation for the Haven hypervisor.
 * Uses LDAXR/STLXR exclusive memory access for SMP safety.
 * The lock value is 0 = free, 1 = held. */

#include <stdint.h>

/* -----------------------------------------------------------------------
 * hv_spin_lock - acquire a spinlock (busy-wait until successful).
 *
 * lock: pointer to a uint32_t initialized to 0.
 * Uses LDAXR/STLXR with Acquire-Release semantics so that
 * memory accesses inside the critical section are not reordered
 * outside the lock/unlock pair.
 * ----------------------------------------------------------------------- */
void hv_spin_lock(volatile uint32_t *lock)
{
	uint32_t tmp;
	uint32_t got;

	__asm__ volatile(
		"1: ldaxr   %w0, [%2]       \n" /* Load-Acquire Exclusive */
		"   cbnz    %w0, 1b         \n" /* Spin if lock != 0 */
		"   stlxr   %w1, %w3, [%2] \n" /* Store-Release Exclusive */
		"   cbnz    %w1, 1b         \n" /* Retry if store failed */
		: "=&r"(tmp), "=&r"(got)
		: "r"(lock), "r"(1U)
		: "memory");
}

/* -----------------------------------------------------------------------
 * hv_spin_trylock - attempt to acquire lock without spinning.
 *
 * Returns 1 if the lock was acquired, 0 if it is already held.
 * ----------------------------------------------------------------------- */
int hv_spin_trylock(volatile uint32_t *lock)
{
	uint32_t tmp;
	uint32_t got;

	__asm__ volatile(
		"   ldaxr   %w0, [%2]       \n"
		"   cbnz    %w0, 2f         \n" /* Already held - bail */
		"   stlxr   %w1, %w3, [%2] \n"
		"   cbz     %w1, 3f         \n" /* Store succeeded */
		"2: mov     %w0, #1         \n" /* Signal: not acquired */
		"   b       4f              \n"
		"3: mov     %w0, #0         \n" /* Signal: acquired */
		"4:                         \n"
		: "=&r"(tmp), "=&r"(got)
		: "r"(lock), "r"(1U)
		: "memory");

	return (tmp == 0) ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * hv_spin_unlock - release a spinlock.
 *
 * Uses STLR (Store-Release) to ensure all stores inside the critical
 * section are visible before the lock word is cleared.
 * ----------------------------------------------------------------------- */
void hv_spin_unlock(volatile uint32_t *lock)
{
	__asm__ volatile(
		"   stlr    wzr, [%0]       \n" /* Store-Release zero */
		:
		: "r"(lock)
		: "memory");
}
