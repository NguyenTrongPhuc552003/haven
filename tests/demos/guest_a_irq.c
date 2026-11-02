/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_a_irq.c - interrupt delivery to partition A (GI2).
 *
 * Partition A owns SPI 33, the PL011 UART interrupt.  This enables the
 * UART's transmit interrupt, prints a line, and waits for INTID 33 to
 * arrive through the virtual CPU interface (Haven injects it, linked to the
 * physical interrupt).  The handler masks the UART interrupt before EOI, so
 * the level-triggered line drops.
 *
 * PL011 registers (Arm PL011 TRM): IMSC 0x038, ICR 0x044, TXIM/TXIC bit 5.
 */

#include <stdint.h>

#define UART_BASE 0x09000000UL
#define UART_FR (UART_BASE + 0x18UL)
#define UART_IMSC (UART_BASE + 0x38UL)
#define UART_ICR (UART_BASE + 0x44UL)
#define UART_TX (1U << 5)
#define UART_INTID 33U
#define POLL_LIMIT 20000000U

static volatile uint32_t irq_count;
static volatile uint32_t irq_other;

static inline void w32(uint64_t a, uint32_t v)
{
	*(volatile uint32_t *)a = v;
}

static inline uint32_t r32(uint64_t a)
{
	return *(volatile uint32_t *)a;
}

static void puts_(const char *s)
{
	while (*s) {
		while (r32(UART_FR) & (1U << 5))
			;
		w32(UART_BASE, (uint32_t)(unsigned char)*s++);
	}
}

void guest_irq_handler(void)
{
	uint64_t iar;

	__asm__ volatile("mrs %0, icc_iar1_el1" : "=r"(iar));
	if ((iar & 0xffffffU) == UART_INTID) {
		w32(UART_IMSC, 0);
		w32(UART_ICR, UART_TX);
		irq_count++;
	} else if ((iar & 0xffffffU) < 1020U) {
		irq_other++;
	}
	__asm__ volatile("msr icc_eoir1_el1, %0\n\tisb" ::"r"(iar));
}

void guest_a_irq_test(void)
{
	extern char guest_vectors[];
	uint64_t sre;

	__asm__ volatile("msr vbar_el1, %0\n\tisb" ::"r"(guest_vectors));
	__asm__ volatile("mrs %0, icc_sre_el1" : "=r"(sre));
	__asm__ volatile("msr icc_sre_el1, %0\n\tisb" ::"r"(sre | 1U));
	__asm__ volatile("msr icc_pmr_el1, %0" ::"r"(0xffUL));
	__asm__ volatile("msr icc_igrpen1_el1, %0\n\tisb" ::"r"(1UL));

	w32(UART_ICR, 0x7ffU);
	w32(UART_IMSC, UART_TX);
	__asm__ volatile("msr daifclr, #2" ::: "memory");
	puts_("PARTITION_A: waiting for the UART interrupt\r\n");
	for (uint32_t i = 0; i < POLL_LIMIT && irq_count == 0U; i++)
		;
	__asm__ volatile("msr daifset, #2" ::: "memory");
	w32(UART_IMSC, 0);

	if (irq_count != 0U && irq_other == 0U)
		puts_("PARTITION_A: UART interrupt delivered (INTID 33)\r\n");
	else if (irq_count != 0U)
		puts_("PARTITION_A: UART interrupt delivered, plus others\r\n");
	else
		puts_("PARTITION_A: UART interrupt NOT delivered\r\n");
	puts_("PARTITION_A: IRQ test done\r\n");
}
