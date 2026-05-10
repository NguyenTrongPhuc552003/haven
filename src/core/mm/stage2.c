#include "haven/stage2.h"

#define HV_MAX_PARTITIONS 256U

static hv_u8 stage2_partition_mapped[HV_MAX_PARTITIONS];

static hv_u8 hv_region_overlaps(const struct hv_mem_region *a,
                                const struct hv_mem_region *b) {
  hv_u64 a_end = a->ipa_base + a->size;
  hv_u64 b_end = b->ipa_base + b->size;

  if (a_end <= b->ipa_base || b_end <= a->ipa_base) {
    return 0U;
  }

  return 1U;
}

hv_status_t hv_stage2_init(void) {
  hv_u32 i;

  for (i = 0U; i < HV_MAX_PARTITIONS; ++i) {
    stage2_partition_mapped[i] = 0U;
  }

  return HV_OK;
}

hv_status_t hv_stage2_map_partition(const struct hv_partition_mem *cfg) {
  hv_u32 i;
  hv_u32 j;

  if (cfg == NULL || cfg->regions == NULL || cfg->region_count == 0U ||
      cfg->partition_id == 0U) {
    return HV_EINVAL;
  }

  if (cfg->partition_id >= HV_MAX_PARTITIONS) {
    return HV_ENOSPC;
  }

  if (stage2_partition_mapped[cfg->partition_id] != 0U) {
    return HV_EPERM;
  }

  for (i = 0U; i < cfg->region_count; ++i) {
    const struct hv_mem_region *region = &cfg->regions[i];

    if (region->size == 0U) {
      return HV_EINVAL;
    }

    if (region->ipa_base + region->size < region->ipa_base ||
        region->pa_base + region->size < region->pa_base) {
      return HV_EINVAL;
    }

    for (j = i + 1U; j < cfg->region_count; ++j) {
      if (hv_region_overlaps(region, &cfg->regions[j]) != 0U) {
        return HV_EINVAL;
      }
    }
  }

  stage2_partition_mapped[cfg->partition_id] = 1U;
  return HV_OK;
}

hv_status_t hv_stage2_unmap_partition(hv_u32 partition_id) {
  if (partition_id == 0U) {
    return HV_EINVAL;
  }

  if (partition_id >= HV_MAX_PARTITIONS) {
    return HV_ENOSPC;
  }

  if (stage2_partition_mapped[partition_id] == 0U) {
    return HV_EPERM;
  }

  stage2_partition_mapped[partition_id] = 0U;
  return HV_OK;
}
