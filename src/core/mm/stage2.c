#include "haven/stage2.h"

hv_status_t hv_stage2_init(void) { return HV_OK; }

hv_status_t hv_stage2_map_partition(const struct hv_partition_mem *cfg) {
  if (cfg == NULL || cfg->regions == NULL || cfg->region_count == 0U) {
    return HV_EINVAL;
  }

  return HV_OK;
}

hv_status_t hv_stage2_unmap_partition(hv_u32 partition_id) {
  if (partition_id == 0U) {
    return HV_EINVAL;
  }

  return HV_OK;
}
