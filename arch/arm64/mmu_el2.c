/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/mmu_el2.c - Haven's own EL2 stage-1 identity map (FINDINGS A1).
 *
 * Static tables, no allocation: one level-1 table (1 GiB per entry) and at
 * most EL2_MAX_L2 level-2 tables (2 MiB per entry). See asm/el2_mmu.h for
 * the memory types and why they were chosen.
 */

#include <asm/el2_mmu.h>
#include <asm/sysregs.h>
#include <stdint.h>

#define EL2_MAX_L2 4U
#define SZ_2M 0x200000UL
#define SZ_1G 0x40000000UL
#define EL2_VA_LIMIT (1UL << 39)

/* Descriptor fields (VMSAv8-64, stage 1, single-range EL2 regime). */
#define D_BLOCK 0x1UL
#define D_TABLE 0x3UL
#define D_ATTR(i) ((uint64_t)(i) << 2)
#define D_AP1 (1UL << 6) /* RES1 in a single-range regime */
#define D_SH_IS (3UL << 8)
#define D_AF (1UL << 10)
#define D_XN (1UL << 54)
#define D_ADDR_MASK 0x0000fffffffff000UL

static uint64_t el2_l1[512] __attribute__((aligned(4096)));
static uint64_t el2_l2[EL2_MAX_L2][512] __attribute__((aligned(4096)));
static uint32_t el2_l2_used;

volatile uint64_t hv_el2_ttbr;

extern char _start[];
extern char __bss_end[];

static uint64_t block_attrs(uint32_t type)
{
	if (type == HV_EL2_NORMAL)
		return D_ATTR(EL2_ATTR_NORMAL) | D_SH_IS | D_AF | D_AP1;
	return D_ATTR(EL2_ATTR_DEVICE) | D_AF | D_AP1 | D_XN;
}

static int map_2m(uint64_t pa, uint32_t type)
{
	uint64_t i1 = pa / SZ_1G;
	uint64_t *l2;

	if ((el2_l1[i1] & 0x3UL) == D_BLOCK)
		return -2; /* overlaps a 1 GiB block */
	if ((el2_l1[i1] & 0x3UL) != D_TABLE) {
		if (el2_l2_used >= EL2_MAX_L2)
			return -3;
		l2 = el2_l2[el2_l2_used++];
		el2_l1[i1] = ((uint64_t)(uintptr_t)l2 & D_ADDR_MASK) | D_TABLE;
	} else {
		l2 = (uint64_t *)(uintptr_t)(el2_l1[i1] & D_ADDR_MASK);
	}
	l2[(pa % SZ_1G) / SZ_2M] = pa | block_attrs(type) | D_BLOCK;
	return 0;
}

int hv_arch_el2_mmu_build(void)
{
	uint64_t img_lo = (uint64_t)(uintptr_t)_start;
	uint64_t img_hi = (uint64_t)(uintptr_t)__bss_end;
	int covered = 0;

	if (hv_el2_region_count == 0U)
		return 0; /* legacy platform: translation stays off */

	for (uint32_t r = 0; r < hv_el2_region_count; r++) {
		const struct hv_el2_region *g = &hv_el2_regions[r];

		if ((g->pa | g->size) & (SZ_2M - 1U) || g->size == 0U ||
		    g->pa + g->size > EL2_VA_LIMIT)
			return -1;
		for (uint64_t pa = g->pa; pa < g->pa + g->size;) {
			if (pa % SZ_1G == 0U && g->pa + g->size - pa >= SZ_1G &&
			    el2_l1[pa / SZ_1G] == 0U) {
				el2_l1[pa / SZ_1G] = pa | block_attrs(g->type) |
						     D_BLOCK;
				pa += SZ_1G;
			} else {
				int rc = map_2m(pa, g->type);

				if (rc != 0)
					return rc;
				pa += SZ_2M;
			}
		}
		if (g->type == HV_EL2_NORMAL && g->pa <= img_lo &&
		    img_hi <= g->pa + g->size)
			covered = 1;
	}
	if (!covered)
		return -4; /* Haven's own image must be Normal memory */
	hv_el2_ttbr = (uint64_t)(uintptr_t)el2_l1;
	return 0;
}

void hv_arch_el2_mmu_enable(void)
{
	uint64_t ctr;
	uint64_t line;
	uint64_t sctlr;

	if (hv_el2_ttbr == 0U)
		return;

	/* Until now every data access was Device (translation off), so memory
	 * is up to date; cache lines left by firmware over the image may be
	 * stale.  Invalidate (not clean) them by address. */
	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
	line = 4UL << ((ctr >> 16) & 0xf);
	for (uint64_t a = (uint64_t)(uintptr_t)_start & ~(line - 1U);
	     a < (uint64_t)(uintptr_t)__bss_end; a += line)
		__asm__ volatile("dc ivac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");

	write_sysreg(EL2_MAIR_VALUE, mair_el2);
	write_sysreg(EL2_TCR_VALUE, tcr_el2);
	write_sysreg(hv_el2_ttbr, ttbr0_el2);
	__asm__ volatile("isb\n\ttlbi alle2\n\tdsb ish\n\tisb" ::: "memory");
	sctlr = read_sysreg(sctlr_el2);
	sctlr |= EL2_SCTLR_M | EL2_SCTLR_C | EL2_SCTLR_I;
	write_sysreg(sctlr, sctlr_el2);
	__asm__ volatile("isb" ::: "memory");
}
