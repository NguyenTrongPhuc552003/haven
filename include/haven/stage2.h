#ifndef HAVEN_STAGE2_H
#define HAVEN_STAGE2_H

#include "types.h"

/* Portable stage-2 permission and memory-type bits for hv_mem_region.attrs.
 * The arch layer translates them into descriptor bits. Every valid value
 * includes HV_MEM_R; attrs == 0 is invalid, so a caller that forgets to state
 * permissions fails at map time instead of receiving an RWX mapping (MM1). */
#define HV_MEM_R (1ULL << 0) /* readable */
#define HV_MEM_W (1ULL << 1) /* writable (requires HV_MEM_R) */
#define HV_MEM_X (1ULL << 2) /* executable (never with HV_MEM_DEVICE) */
#define HV_MEM_DEVICE (1ULL << 3) /* Device nGnRE; clear = Normal WB */
#define HV_MEM_VALID_MASK (HV_MEM_R | HV_MEM_W | HV_MEM_X | HV_MEM_DEVICE)

#define HV_MEM_RWX (HV_MEM_R | HV_MEM_W | HV_MEM_X) /* guest RAM */
#define HV_MEM_DEVICE_RW (HV_MEM_DEVICE | HV_MEM_R | HV_MEM_W) /* MMIO */

struct hv_mem_region {
	hv_u64 ipa_base;
	hv_u64 pa_base;
	hv_u64 size;
	hv_u64 attrs; /* HV_MEM_* bits */
};

struct hv_partition_mem {
	hv_u32 partition_id;
	const struct hv_mem_region *regions;
	hv_u32 region_count;
};

hv_u8 hv_stage2_attrs_valid(hv_u64 attrs);
hv_status_t hv_stage2_init(void);
hv_status_t hv_stage2_map_partition(const struct hv_partition_mem *cfg);
hv_status_t hv_stage2_add_regions(const struct hv_partition_mem *cfg);
hv_status_t hv_stage2_unmap_partition(hv_u32 partition_id);
hv_status_t hv_stage2_partition_contains_pa(hv_u32 partition_id, hv_u64 pa_base,
					    hv_u64 size);

#endif
