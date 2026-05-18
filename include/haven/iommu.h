/**
 * IOMMU group ownership policy interface.
 *
 * Enforces partition-level IOMMU group assignment: each IOMMU group is
 * owned by exactly one partition at a time. Assignment without a prior
 * release by the current owner is denied (fail-closed). Group 0 and
 * partition 0 are reserved and always rejected.
 *
 * Relationship to SMMU module:
 *   The SMMU module manages stream IDs and DMA translation windows at
 *   hardware level. This module sits above it, governing which group of
 *   devices (by IOMMU group ID) a partition is allowed to control. A
 *   partition must own an IOMMU group before its SMMU stream IDs can be
 *   considered authoritative for that device group.
 *
 * Isolation invariants enforced:
 *   - Partition 0 is reserved and always rejected.
 *   - Group 0 is reserved and always rejected.
 *   - One owner per group; reassignment without release returns HV_EPERM.
 *   - Double-release returns HV_EPERM.
 *   - hv_iommu_check_group returns HV_EPERM if caller is not the owner.
 *
 * @file include/haven/iommu.h
 */

#ifndef HAVEN_IOMMU_H
#define HAVEN_IOMMU_H

#include <haven/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum IOMMU groups tracked.
 */
#define HV_MAX_IOMMU_GROUPS 256U

/**
 * Maximum partition IDs (0 reserved, 1..255 valid).
 */
#define HV_MAX_IOMMU_PARTITIONS 256U

/**
     * Initialize the IOMMU policy subsystem.
     *
     * Clears all group-to-partition assignments.
     *
     * @return HV_OK on success.
     */
hv_status_t hv_iommu_init(void);

/**
     * Assign an IOMMU group to a partition.
     *
     * The group must not already be assigned to a different partition. If
     * the group is already assigned to the same partition, this is a no-op
     * and returns HV_OK (idempotent).
     *
     * @param group_id      IOMMU group identifier (1–255; 0 is invalid).
     * @param partition_id  Owning partition (1–255; 0 is invalid).
     *
     * @return HV_OK      Assignment succeeded.
     * @return HV_EINVAL  group_id == 0 or partition_id == 0.
     * @return HV_EPERM   Group already assigned to a different partition.
     */
hv_status_t hv_iommu_assign_group(hv_u32 group_id, hv_u32 partition_id);

/**
     * Release an IOMMU group from its owning partition.
     *
     * Only the current owner may release the group.
     *
     * @param group_id      IOMMU group identifier (1–255).
     * @param partition_id  Partition claiming ownership (1–255).
     *
     * @return HV_OK      Group released.
     * @return HV_EINVAL  group_id == 0 or partition_id == 0.
     * @return HV_EPERM   Group not assigned, or caller is not the owner.
     */
hv_status_t hv_iommu_release_group(hv_u32 group_id, hv_u32 partition_id);

/**
     * Check whether a partition owns an IOMMU group.
     *
     * @param group_id      IOMMU group identifier (1–255).
     * @param partition_id  Partition to check (1–255).
     *
     * @return HV_OK      Partition is the owner.
     * @return HV_EINVAL  group_id == 0 or partition_id == 0.
     * @return HV_EPERM   Group unassigned or owned by a different partition.
     */
hv_status_t hv_iommu_check_group(hv_u32 group_id, hv_u32 partition_id);

#ifdef __cplusplus
}
#endif

#endif /* HAVEN_IOMMU_H */
