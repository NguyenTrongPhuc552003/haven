/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_mmu.c - EL1 stage-1 identity map for the demo guests.
 *
 * Without it every guest access is Device memory (stage 1 off), so a guest
 * never uses the caches and cannot create or suffer cache interference,
 * which the interference measurements need
 * (docs/methodology/stage2/20261102-interference/PLAN.md).
 *
 * One level-1 table, 39-bit VA, 4 KiB granule, 1 GiB blocks:
 *   IPA 0x00_0000_0000 + 1 GiB  Device-nGnRE (UART, PCI BAR window)
 *   IPA 0x00_4000_0000 + 1 GiB  Normal write-back (both partitions' RAM;
 *                               stage 2 still decides what each may reach)
 *   IPA 0x40_0000_0000 + 1 GiB  Device-nGnRE (PCI configuration page)
 * Stage 2 combines with this: partition RAM is Normal write-back there too.
 */

#include <stdint.h>

#define D_BLOCK 0x1UL
#define D_ATTR(i) ((uint64_t)(i) << 2)
#define D_SH_IS (3UL << 8)
#define D_AF (1UL << 10)
#define D_UXN (1UL << 54)
#define D_PXN (1UL << 53)
#define DEVICE (D_ATTR(0) | D_AF | D_UXN | D_PXN | D_BLOCK)
#define NORMAL (D_ATTR(1) | D_SH_IS | D_AF | D_BLOCK)

/* MAIR_EL1: Attr0 Device-nGnRE, Attr1 Normal WB RA WA. */
#define MAIR 0xff04UL
/* TCR_EL1: T0SZ 25, IRGN0 = ORGN0 = WB, SH0 IS, TG0 4 KiB, EPD1 (no
 * TTBR1 walks), IPS 40-bit. */
#define TCR                                                            \
	(25UL | (1UL << 8) | (1UL << 10) | (3UL << 12) | (1UL << 23) | \
	 (2UL << 32))

static uint64_t l1[512] __attribute__((aligned(4096)));

extern char __guest_start[];
extern char __guest_end[];

void guest_mmu_enable(void)
{
	uint64_t ctr, line, sctlr;

	l1[0] = 0x0UL | DEVICE;
	l1[1] = 0x40000000UL | NORMAL;
	l1[256] = 0x4000000000UL | DEVICE;

	/* Data written so far went straight to memory (Device); discard any
	 * stale cache lines over the image before turning caches on. */
	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
	line = 4UL << ((ctr >> 16) & 0xf);
	for (uint64_t a = (uint64_t)(uintptr_t)__guest_start & ~(line - 1U);
	     a < (uint64_t)(uintptr_t)__guest_end; a += line)
		__asm__ volatile("dc ivac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");

	__asm__ volatile("msr mair_el1, %0" ::"r"(MAIR));
	__asm__ volatile("msr tcr_el1, %0" ::"r"(TCR));
	__asm__ volatile("msr ttbr0_el1, %0" ::"r"((uint64_t)(uintptr_t)l1));
	__asm__ volatile("isb\n\ttlbi vmalle1\n\tdsb nsh\n\tisb" ::: "memory");
	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr |= (1UL << 0) | (1UL << 2) | (1UL << 12); /* M, C, I */
	__asm__ volatile("msr sctlr_el1, %0\n\tisb" ::"r"(sctlr) : "memory");
}
