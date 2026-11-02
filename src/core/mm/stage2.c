#include "haven/stage2.h"

#ifdef HAVEN_ARCH_ARM64
extern hv_status_t hv_arch_stage2_init_partition(hv_u32 partition_id);
extern hv_status_t hv_arch_stage2_map(hv_u32 partition_id, hv_u64 ipa,
				      hv_u64 pa, hv_u64 size, hv_u32 flags);
extern hv_status_t hv_arch_stage2_unmap(hv_u32 partition_id, hv_u64 ipa,
					hv_u64 size);
extern void hv_arch_stage2_flush_tlb(void);
#endif

#define HV_MAX_PARTITIONS 256U
#define HV_MAX_PARTITION_REGIONS 64U
#define HV_STAGE2_PAGE_SIZE 0x1000ULL
#define HV_STAGE2_PAGE_MASK (HV_STAGE2_PAGE_SIZE - 1ULL)

static hv_u8 stage2_partition_mapped[HV_MAX_PARTITIONS];
static struct hv_mem_region stage2_partition_regions[HV_MAX_PARTITIONS]
						    [HV_MAX_PARTITION_REGIONS];
static hv_u32 stage2_partition_region_count[HV_MAX_PARTITIONS];

static hv_u8 hv_range_overlaps(hv_u64 base_a, hv_u64 size_a, hv_u64 base_b,
			       hv_u64 size_b)
{
	hv_u64 end_a = base_a + size_a;
	hv_u64 end_b = base_b + size_b;

	if (end_a <= base_b || end_b <= base_a) {
		return 0U;
	}

	return 1U;
}

static hv_u8 hv_region_overlaps_ipa(const struct hv_mem_region *a,
				    const struct hv_mem_region *b)
{
	return hv_range_overlaps(a->ipa_base, a->size, b->ipa_base, b->size);
}

static hv_u8 hv_region_overlaps_pa(const struct hv_mem_region *a,
				   const struct hv_mem_region *b)
{
	return hv_range_overlaps(a->pa_base, a->size, b->pa_base, b->size);
}

static hv_u8 hv_is_page_aligned(hv_u64 value)
{
	return (value & HV_STAGE2_PAGE_MASK) == 0U;
}

static hv_u8 hv_range_contains(hv_u64 base, hv_u64 size, hv_u64 inner_base,
			       hv_u64 inner_size)
{
	hv_u64 end = base + size;
	hv_u64 inner_end = inner_base + inner_size;

	if (inner_end < inner_base || end < base) {
		return 0U;
	}

	if (inner_base < base || inner_end > end) {
		return 0U;
	}

	return 1U;
}

/* Return 1 when attrs is a valid HV_MEM_* combination (MM1):
 *   - no bits outside HV_MEM_VALID_MASK;
 *   - HV_MEM_R present (this also rejects 0, write-only and execute-only);
 *   - never HV_MEM_DEVICE together with HV_MEM_X. */
hv_u8 hv_stage2_attrs_valid(hv_u64 attrs)
{
	if ((attrs & ~HV_MEM_VALID_MASK) != 0U) {
		return 0U;
	}

	if ((attrs & HV_MEM_R) == 0U) {
		return 0U;
	}

	if ((attrs & HV_MEM_DEVICE) != 0U && (attrs & HV_MEM_X) != 0U) {
		return 0U;
	}

	return 1U;
}

hv_status_t hv_stage2_init(void)
{
	hv_u32 i;

	for (i = 0U; i < HV_MAX_PARTITIONS; ++i) {
		stage2_partition_mapped[i] = 0U;
		stage2_partition_region_count[i] = 0U;
	}

	return HV_OK;
}

/* Check regions about to be added to a partition: valid attributes, page
 * alignment, no wrap, no IPA overlap with each other or with the
 * partition's existing regions, and no PA overlap with another partition. */
static hv_status_t validate_new_regions(hv_u32 partition_id,
					const struct hv_mem_region *regions,
					hv_u32 count)
{
	hv_u32 i;
	hv_u32 j;

	for (i = 0U; i < count; ++i) {
		const struct hv_mem_region *region = &regions[i];

		if (region->size == 0U) {
			return HV_EINVAL;
		}

		if (hv_stage2_attrs_valid(region->attrs) == 0U) {
			return HV_EINVAL;
		}

		if (hv_is_page_aligned(region->ipa_base) == 0U ||
		    hv_is_page_aligned(region->pa_base) == 0U ||
		    hv_is_page_aligned(region->size) == 0U) {
			return HV_EINVAL;
		}

		if (region->ipa_base + region->size < region->ipa_base ||
		    region->pa_base + region->size < region->pa_base) {
			return HV_EINVAL;
		}

		for (j = i + 1U; j < count; ++j) {
			if (hv_region_overlaps_ipa(region, &regions[j]) != 0U) {
				return HV_EINVAL;
			}
		}

		for (j = 0U; j < stage2_partition_region_count[partition_id];
		     ++j) {
			if (hv_region_overlaps_ipa(
				    region,
				    &stage2_partition_regions[partition_id][j]) !=
			    0U) {
				return HV_EINVAL;
			}
		}

		for (j = 1U; j < HV_MAX_PARTITIONS; ++j) {
			hv_u32 k;

			if (j == partition_id ||
			    stage2_partition_mapped[j] == 0U) {
				continue;
			}

			for (k = 0U; k < stage2_partition_region_count[j];
			     ++k) {
				if (hv_region_overlaps_pa(
					    region,
					    &stage2_partition_regions[j][k]) !=
				    0U) {
					return HV_EPERM;
				}
			}
		}
	}

	return HV_OK;
}

hv_status_t hv_stage2_map_partition(const struct hv_partition_mem *cfg)
{
	hv_u32 i;

	if (cfg == NULL || cfg->regions == NULL || cfg->region_count == 0U ||
	    cfg->partition_id == 0U) {
		return HV_EINVAL;
	}

	if (cfg->partition_id >= HV_MAX_PARTITIONS) {
		return HV_ENOSPC;
	}

	if (cfg->region_count > HV_MAX_PARTITION_REGIONS) {
		return HV_ENOSPC;
	}

	if (stage2_partition_mapped[cfg->partition_id] != 0U) {
		return HV_EPERM;
	}

	{
		hv_status_t rc = validate_new_regions(
			cfg->partition_id, cfg->regions, cfg->region_count);
		if (rc != HV_OK) {
			return rc;
		}
	}

	for (i = 0U; i < cfg->region_count; ++i) {
		stage2_partition_regions[cfg->partition_id][i] =
			cfg->regions[i];
	}
	stage2_partition_region_count[cfg->partition_id] = cfg->region_count;
	stage2_partition_mapped[cfg->partition_id] = 1U;

#ifdef HAVEN_ARCH_ARM64
	{
		hv_status_t arch_rc =
			hv_arch_stage2_init_partition(cfg->partition_id);
		if (arch_rc != HV_OK) {
			stage2_partition_region_count[cfg->partition_id] = 0U;
			stage2_partition_mapped[cfg->partition_id] = 0U;
			return arch_rc;
		}

		for (i = 0U; i < cfg->region_count; ++i) {
			const struct hv_mem_region *region = &cfg->regions[i];
			arch_rc = hv_arch_stage2_map(cfg->partition_id,
						     region->ipa_base,
						     region->pa_base,
						     region->size,
						     (hv_u32)region->attrs);
			if (arch_rc != HV_OK) {
				hv_u32 rollback_i;
				for (rollback_i = 0U; rollback_i < i;
				     ++rollback_i) {
					const struct hv_mem_region *mapped =
						&cfg->regions[rollback_i];
					hv_arch_stage2_unmap(cfg->partition_id,
							     mapped->ipa_base,
							     mapped->size);
				}
				hv_arch_stage2_flush_tlb();
				stage2_partition_region_count[cfg->partition_id] =
					0U;
				stage2_partition_mapped[cfg->partition_id] = 0U;
				return arch_rc;
			}
		}
	}
#endif

	return HV_OK;
}

/* Add regions to a partition that is already mapped, e.g. the registers of
 * a device whose DMA grant has been enforced.  Same checks as
 * hv_stage2_map_partition(); on failure nothing is added. */
hv_status_t hv_stage2_add_regions(const struct hv_partition_mem *cfg)
{
	hv_u32 i;
	hv_u32 base;
	hv_status_t rc;

	if (cfg == NULL || cfg->regions == NULL || cfg->region_count == 0U ||
	    cfg->partition_id == 0U) {
		return HV_EINVAL;
	}

	if (cfg->partition_id >= HV_MAX_PARTITIONS) {
		return HV_ENOSPC;
	}

	if (stage2_partition_mapped[cfg->partition_id] == 0U) {
		return HV_EPERM;
	}

	base = stage2_partition_region_count[cfg->partition_id];
	if (cfg->region_count > HV_MAX_PARTITION_REGIONS - base) {
		return HV_ENOSPC;
	}

	rc = validate_new_regions(cfg->partition_id, cfg->regions,
				  cfg->region_count);
	if (rc != HV_OK) {
		return rc;
	}

#ifdef HAVEN_ARCH_ARM64
	for (i = 0U; i < cfg->region_count; ++i) {
		const struct hv_mem_region *region = &cfg->regions[i];

		rc = hv_arch_stage2_map(cfg->partition_id, region->ipa_base,
					region->pa_base, region->size,
					(hv_u32)region->attrs);
		if (rc != HV_OK) {
			hv_u32 k;

			for (k = 0U; k < i; ++k) {
				hv_arch_stage2_unmap(cfg->partition_id,
						     cfg->regions[k].ipa_base,
						     cfg->regions[k].size);
			}
			hv_arch_stage2_flush_tlb();
			return rc;
		}
	}
#endif

	for (i = 0U; i < cfg->region_count; ++i) {
		stage2_partition_regions[cfg->partition_id][base + i] =
			cfg->regions[i];
	}
	stage2_partition_region_count[cfg->partition_id] =
		base + cfg->region_count;
	return HV_OK;
}

hv_status_t hv_stage2_unmap_partition(hv_u32 partition_id)
{
	if (partition_id == 0U) {
		return HV_EINVAL;
	}

	if (partition_id >= HV_MAX_PARTITIONS) {
		return HV_ENOSPC;
	}

	if (stage2_partition_mapped[partition_id] == 0U) {
		return HV_EPERM;
	}

#ifdef HAVEN_ARCH_ARM64
	{
		hv_u32 k;
		for (k = 0U; k < stage2_partition_region_count[partition_id];
		     ++k) {
			const struct hv_mem_region *region =
				&stage2_partition_regions[partition_id][k];
			hv_arch_stage2_unmap(partition_id, region->ipa_base,
					     region->size);
		}
		hv_arch_stage2_flush_tlb();
	}
#endif

	stage2_partition_region_count[partition_id] = 0U;
	stage2_partition_mapped[partition_id] = 0U;
	return HV_OK;
}

hv_status_t hv_stage2_partition_contains_pa(hv_u32 partition_id, hv_u64 pa_base,
					    hv_u64 size)
{
	hv_u32 i;

	if (partition_id == 0U || partition_id >= HV_MAX_PARTITIONS ||
	    size == 0U) {
		return HV_EINVAL;
	}

	if (stage2_partition_mapped[partition_id] == 0U) {
		return HV_EPERM;
	}

	for (i = 0U; i < stage2_partition_region_count[partition_id]; ++i) {
		const struct hv_mem_region *region =
			&stage2_partition_regions[partition_id][i];

		if (hv_range_contains(region->pa_base, region->size, pa_base,
				      size) != 0U) {
			return HV_OK;
		}
	}

	return HV_EPERM;
}
