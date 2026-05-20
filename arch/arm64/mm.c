/* SPDX-License-Identifier: Apache-2.0 */
/* ARM64 Stage-2 page table hardware programming for the Haven hypervisor.
 *
 * This layer bridges the architecture-neutral policy in src/core/mm/stage2.c
 * (which validates ownership and overlap) to the hardware: it allocates and
 * programs actual page-table descriptor trees and programs VTTBR_EL2.
 *
 * Design constraints (matching Haven TCB story):
 *   - Static allocation only: no heap, no malloc.
 *   - Fixed page-table pool: HV_ARCH_PT_POOL_PAGES pages preallocated in BSS.
 *   - 4KB granule, 3-level walk (L1→L2→L3), 39-bit IPA (T0SZ=25).
 *   - Each partition gets its own root (L1) table and up to
 *     HV_ARCH_MAX_PT_PAGES additional L2/L3 tables shared from the pool.
 */

#include <asm/page.h>
#include <asm/sysregs.h>
#include <haven/types.h>
#include <stddef.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Configuration
 * ----------------------------------------------------------------------- */

#define HV_ARCH_MAX_PARTITIONS 256
#define HV_ARCH_PT_POOL_PAGES 1024 /* 1024 × 4KB = 4MB pool for all tables */

/* -----------------------------------------------------------------------
 * Page-table pool (static allocation in BSS)
 * Each "page" here is one 4KB translation table (512 × uint64_t entries).
 * ----------------------------------------------------------------------- */

typedef uint64_t hv_pt_page_t[S2_PTRS_PER_LEVEL];

static hv_pt_page_t pt_pool[HV_ARCH_PT_POOL_PAGES]
	__attribute__((aligned(HV_PAGE_SIZE)));
static uint32_t pt_pool_next; /* next free page index */

/* Per-partition L1 root table pointer (NULL = partition not initialized) */
static hv_pt_page_t *pt_roots[HV_ARCH_MAX_PARTITIONS];

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

static hv_pt_page_t *pt_alloc(void)
{
	if (pt_pool_next >= HV_ARCH_PT_POOL_PAGES)
		return NULL;
	hv_pt_page_t *p = &pt_pool[pt_pool_next++];
	/* Zero the page (BSS is zero-initialized, but re-use after free isn't
   * supported - this is a static allocator without free). */
	for (int i = 0; i < S2_PTRS_PER_LEVEL; i++)
		(*p)[i] = 0;
	return p;
}

static inline uintptr_t pt_pa(const hv_pt_page_t *pt)
{
	return (uintptr_t)pt;
}

/* -----------------------------------------------------------------------
 * hv_arch_stage2_init_partition - allocate the L1 root table for a partition.
 *
 * Must be called once per partition before any mapping.
 * Returns HV_OK on success, HV_ENOMEM if pool is exhausted.
 * ----------------------------------------------------------------------- */
hv_status_t hv_arch_stage2_init_partition(uint32_t partition_id)
{
	if (partition_id == 0 || partition_id >= HV_ARCH_MAX_PARTITIONS)
		return HV_EINVAL;

	if (pt_roots[partition_id] != NULL)
		return HV_OK; /* Already initialized */

	hv_pt_page_t *root = pt_alloc();
	if (root == NULL)
		return HV_ENOMEM;

	pt_roots[partition_id] = root;
	return HV_OK;
}

/* -----------------------------------------------------------------------
 * hv_arch_stage2_map - add an IPA→PA mapping to a partition's page tables.
 *
 * ipa:   guest-visible base address (must be HV_PAGE_SIZE aligned)
 * pa:    host physical base address (must be HV_PAGE_SIZE aligned)
 * size:  mapping size in bytes (must be HV_PAGE_SIZE multiple)
 * flags: S2AP_* | S2_MEMATTR_* | S2_SH_* from page.h
 *
 * Uses 4KB pages for all sizes (simple and safe; 2MB blocks are an
 * optimization that can be added later without changing the policy layer).
 *
 * Returns HV_OK on success, HV_ENOMEM if page-table pool exhausted.
 * ----------------------------------------------------------------------- */
hv_status_t hv_arch_stage2_map(uint32_t partition_id, uint64_t ipa, uint64_t pa,
			       uint64_t size, uint32_t flags)
{
	if (partition_id == 0 || partition_id >= HV_ARCH_MAX_PARTITIONS)
		return HV_EINVAL;
	if (pt_roots[partition_id] == NULL)
		return HV_EINVAL;
	if ((ipa & (HV_PAGE_SIZE - 1)) || (pa & (HV_PAGE_SIZE - 1)) ||
	    (size & (HV_PAGE_SIZE - 1)))
		return HV_EINVAL;

	hv_pt_page_t *l1 = pt_roots[partition_id];
	uint64_t cur_ipa = ipa;
	uint64_t cur_pa = pa;
	uint64_t end_ipa = ipa + size;

	/* Translate flags to hardware S2AP+MemAttr once (same for all pages). */
	uint64_t hw_attrs;
	if (flags == 1U) {
		/* Device MMIO: nGnRE, non-executable, non-cacheable */
		hw_attrs = (uint64_t)(S2_MEMATTR_DEVICE_nGnRE | S2_SH_NS |
				      S2AP_RW) |
			   S2_XN_ALL;
	} else {
		/* Normal DRAM: WB cacheable, inner-shareable, RW */
		hw_attrs =
			(uint64_t)(S2_MEMATTR_NORMAL_WB | S2_SH_IS | S2AP_RW);
	}

	while (cur_ipa < end_ipa) {
		uint32_t l1_idx = s2_l1_index(cur_ipa);
		uint32_t l2_idx = s2_l2_index(cur_ipa);

		/* L1: ensure table entry exists */
		if (!s2_desc_is_table((*l1)[l1_idx])) {
			hv_pt_page_t *l2 = pt_alloc();
			if (l2 == NULL)
				return HV_ENOMEM;
			(*l1)[l1_idx] = S2_TABLE(pt_pa(l2));
		}
		hv_pt_page_t *l2 = (hv_pt_page_t *)(s2_desc_pa((*l1)[l1_idx]));

		/* 2MB block optimisation: use an L2 block descriptor when the
		 * IPA, PA and remaining size are all 2MB-aligned and the L2
		 * slot is not already a TABLE (which would require an L3 walk).
		 * This reduces page-table pool consumption by up to 512× for
		 * large normal-memory regions (e.g. 512 MB DRAM = 1 block
		 * descriptor vs 131 072 page descriptors). */
		if (!(cur_ipa & (HV_BLOCK_SIZE_L2 - 1)) &&
		    !(cur_pa & (HV_BLOCK_SIZE_L2 - 1)) &&
		    (end_ipa - cur_ipa) >= HV_BLOCK_SIZE_L2 &&
		    !s2_desc_is_table((*l2)[l2_idx])) {
			/* Write 2MB L2 block descriptor */
			(*l2)[l2_idx] = (cur_pa & HV_BLOCK_MASK_L2) |
					S2_DESC_BLOCK | hw_attrs | S2_AF;
			cur_ipa += HV_BLOCK_SIZE_L2;
			cur_pa += HV_BLOCK_SIZE_L2;
			continue;
		}

		/* Fall through to 4KB page mapping */
		uint32_t l3_idx = s2_l3_index(cur_ipa);

		/* L2: ensure table entry exists (may upgrade INVALID → TABLE) */
		if (!s2_desc_is_table((*l2)[l2_idx])) {
			hv_pt_page_t *l3 = pt_alloc();
			if (l3 == NULL)
				return HV_ENOMEM;
			(*l2)[l2_idx] = S2_TABLE(pt_pa(l3));
		}
		hv_pt_page_t *l3 = (hv_pt_page_t *)(s2_desc_pa((*l2)[l2_idx]));

		/* L3: write 4KB page descriptor */
		(*l3)[l3_idx] = (cur_pa & HV_PAGE_MASK) | S2_DESC_PAGE |
				hw_attrs | S2_AF;

		cur_ipa += HV_PAGE_SIZE;
		cur_pa += HV_PAGE_SIZE;
	}

	/* Ensure all descriptor writes are visible before enabling */
	dsb_sy();
	return HV_OK;
}

/* -----------------------------------------------------------------------
 * hv_arch_stage2_unmap - remove all mappings for a partition (full teardown).
 *
 * Does not free page-table pages (static pool without free support).
 * Marks all L3 descriptors as invalid; L1/L2 tables remain but are harmless.
 * ----------------------------------------------------------------------- */
hv_status_t hv_arch_stage2_unmap(uint32_t partition_id, uint64_t ipa,
				 uint64_t size)
{
	if (partition_id == 0 || partition_id >= HV_ARCH_MAX_PARTITIONS)
		return HV_EINVAL;
	if (pt_roots[partition_id] == NULL)
		return HV_OK; /* Nothing to unmap */

	hv_pt_page_t *l1 = pt_roots[partition_id];
	uint64_t cur_ipa = ipa;
	uint64_t end_ipa = ipa + size;

	while (cur_ipa < end_ipa) {
		uint32_t l1_idx = s2_l1_index(cur_ipa);
		uint32_t l2_idx = s2_l2_index(cur_ipa);

		if (!s2_desc_is_table((*l1)[l1_idx])) {
			/* Skip ahead by 1 GiB if no L2 table present */
			cur_ipa =
				(cur_ipa & HV_BLOCK_MASK_L1) + HV_BLOCK_SIZE_L1;
			continue;
		}
		hv_pt_page_t *l2 = (hv_pt_page_t *)(s2_desc_pa((*l1)[l1_idx]));

		/* If the L2 slot holds a 2MB block descriptor, invalidate it
		 * and advance by 2MB. */
		if (s2_desc_is_block((*l2)[l2_idx])) {
			(*l2)[l2_idx] = S2_DESC_INVALID;
			cur_ipa =
				(cur_ipa & HV_BLOCK_MASK_L2) + HV_BLOCK_SIZE_L2;
			continue;
		}

		if (!s2_desc_is_table((*l2)[l2_idx])) {
			/* No L3 table; skip 2MB slot */
			cur_ipa =
				(cur_ipa & HV_BLOCK_MASK_L2) + HV_BLOCK_SIZE_L2;
			continue;
		}

		uint32_t l3_idx = s2_l3_index(cur_ipa);
		hv_pt_page_t *l3 = (hv_pt_page_t *)(s2_desc_pa((*l2)[l2_idx]));

		(*l3)[l3_idx] = S2_DESC_INVALID;
		cur_ipa += HV_PAGE_SIZE;
	}

	dsb_sy();
	return HV_OK;
}

/* -----------------------------------------------------------------------
 * hv_arch_stage2_enable - activate stage-2 translation for a partition.
 *
 * Programs VTTBR_EL2 with the partition's root table PA and VMID.
 * TLB is invalidated for the new VMID before enabling.
 * Must be called before ERET to the partition's first instruction.
 * ----------------------------------------------------------------------- */
void hv_arch_stage2_enable(uint32_t partition_id, uint32_t vmid)
{
	if (partition_id >= HV_ARCH_MAX_PARTITIONS ||
	    pt_roots[partition_id] == NULL)
		return;

	uint64_t baddr = pt_pa(pt_roots[partition_id]);
	uint64_t vttbr = vttbr_encode(baddr, vmid);

	/* Invalidate all stage-2 TLB entries for this VMID before activating */
	write_sysreg(vttbr, vttbr_el2);
	isb();
	tlbi_vmalls12e1is();
	dsb_ish();
	isb();
}

/* -----------------------------------------------------------------------
 * hv_arch_stage2_flush_tlb - flush stage-2 TLBs for the current VMID.
 *
 * Called after changing page-table entries for a running partition.
 * ----------------------------------------------------------------------- */
void hv_arch_stage2_flush_tlb(void)
{
	dsb_ish();
	tlbi_vmalls12e1is();
	dsb_ish();
	isb();
}

/* -----------------------------------------------------------------------
 * hv_arch_stage2_pool_free_pages - return remaining pool pages for debug.
 * ----------------------------------------------------------------------- */
uint32_t hv_arch_stage2_pool_free_pages(void)
{
	return HV_ARCH_PT_POOL_PAGES - pt_pool_next;
}

/*
 * hv_arch_stage2_vttbr - return the VTTBR_EL2 value for a partition.
 *
 * Used by the SMMU driver to program STE.S2TTB with the same stage-2
 * table that the CPU's VTTBR_EL2 uses, ensuring DMA and CPU translations
 * are coherent for the same partition VMID.
 *
 * Returns 0 if the partition has no stage-2 table yet.
 */
uint64_t hv_arch_stage2_vttbr(uint32_t partition_id, uint32_t vmid)
{
	if (partition_id >= HV_ARCH_MAX_PARTITIONS ||
	    pt_roots[partition_id] == NULL)
		return 0;

	return vttbr_encode((uintptr_t)pt_roots[partition_id], vmid);
}
