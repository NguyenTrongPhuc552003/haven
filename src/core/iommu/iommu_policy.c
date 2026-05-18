/**
 * IOMMU group ownership policy implementation.
 *
 * @file src/core/iommu/iommu_policy.c
 */

#include <haven/iommu.h>
#include <haven/string.h>

/**
 * Per-group assignment record.
 */
typedef struct {
	hv_u32 partition_id; /**< Owning partition. 0 = unassigned. */
	int assigned; /**< 1 if group is currently assigned. */
} iommu_group_entry_t;

/**
 * Group ownership table.
 */
static iommu_group_entry_t group_table[HV_MAX_IOMMU_GROUPS];

hv_status_t hv_iommu_init(void)
{
	memset(group_table, 0, sizeof(group_table));
	return HV_OK;
}

hv_status_t hv_iommu_assign_group(hv_u32 group_id, hv_u32 partition_id)
{
	if (group_id == 0 || group_id >= HV_MAX_IOMMU_GROUPS) {
		return HV_EINVAL;
	}

	if (partition_id == 0 || partition_id >= HV_MAX_IOMMU_PARTITIONS) {
		return HV_EINVAL;
	}

	iommu_group_entry_t *g = &group_table[group_id];

	/* Already assigned to this partition - idempotent. */
	if (g->assigned && g->partition_id == partition_id) {
		return HV_OK;
	}

	/* Already assigned to a different partition - deny. */
	if (g->assigned) {
		return HV_EPERM;
	}

	g->partition_id = partition_id;
	g->assigned = 1;

	return HV_OK;
}

hv_status_t hv_iommu_release_group(hv_u32 group_id, hv_u32 partition_id)
{
	if (group_id == 0 || group_id >= HV_MAX_IOMMU_GROUPS) {
		return HV_EINVAL;
	}

	if (partition_id == 0 || partition_id >= HV_MAX_IOMMU_PARTITIONS) {
		return HV_EINVAL;
	}

	iommu_group_entry_t *g = &group_table[group_id];

	/* Not assigned at all. */
	if (!g->assigned) {
		return HV_EPERM;
	}

	/* Caller is not the owner. */
	if (g->partition_id != partition_id) {
		return HV_EPERM;
	}

	g->partition_id = 0;
	g->assigned = 0;

	return HV_OK;
}

hv_status_t hv_iommu_check_group(hv_u32 group_id, hv_u32 partition_id)
{
	if (group_id == 0 || group_id >= HV_MAX_IOMMU_GROUPS) {
		return HV_EINVAL;
	}

	if (partition_id == 0 || partition_id >= HV_MAX_IOMMU_PARTITIONS) {
		return HV_EINVAL;
	}

	iommu_group_entry_t *g = &group_table[group_id];

	if (!g->assigned || g->partition_id != partition_id) {
		return HV_EPERM;
	}

	return HV_OK;
}
