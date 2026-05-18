#ifndef HAVEN_STAGE2_H
#define HAVEN_STAGE2_H

#include "types.h"

struct hv_mem_region
{
    hv_u64 ipa_base;
    hv_u64 pa_base;
    hv_u64 size;
    hv_u64 attrs;
};

struct hv_partition_mem
{
    hv_u32 partition_id;
    const struct hv_mem_region *regions;
    hv_u32 region_count;
};

hv_status_t hv_stage2_init(void);
hv_status_t hv_stage2_map_partition(const struct hv_partition_mem *cfg);
hv_status_t hv_stage2_unmap_partition(hv_u32 partition_id);
hv_status_t hv_stage2_partition_contains_pa(hv_u32 partition_id,
                                            hv_u64 pa_base,
                                            hv_u64 size);

#endif
