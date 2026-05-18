/**
 * SMMU (System Memory Management Unit) interface for DMA isolation.
 *
 * Provides DMA isolation enforcement through IOMMU/SMMU translation tables,
 * preventing devices from accessing memory outside assigned partition regions.
 *
 * Each device is assigned a StreamID (unique per device), which maps to a
 * partition's DMA window via the SMMU translation tables.
 *
 * Status codes:
 *   HV_OK              - Success
 *   HV_EINVAL          - Invalid arguments (invalid StreamID, region overlap, etc.)
 *   HV_EPERM           - Permission denied (device not owned by partition, etc.)
 *   HV_ENOSPC          - No space (max devices or partitions reached)
 *   HV_ENOTSUP         - Not supported (SMMU not available on platform)
 *
 * @file include/haven/smmu.h
 */

#ifndef HAVEN_SMMU_H
#define HAVEN_SMMU_H

#include <haven/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum number of devices that can be managed via SMMU.
 */
#define HV_MAX_SMMU_DEVICES 64

/**
 * Maximum StreamIDs per partition (for multi-function devices).
 */
#define HV_MAX_STREAMIDS_PER_PARTITION 16

/**
 * Maximum SMMU-managed partitions.
 */
#define HV_MAX_SMMU_PARTITIONS 256

/**
 * Device DMA access attributes.
 */
typedef enum {
	HV_DMA_RO = 1, /**< Read-only DMA access */
	HV_DMA_WO = 2, /**< Write-only DMA access */
	HV_DMA_RW = 3, /**< Read-write DMA access */
} hv_dma_access_t;

/**
 * SMMU partition DMA window configuration.
 */
typedef struct {
	hv_u64 dma_base; /**< DMA address (IOVA) base */
	hv_u64 dma_size; /**< DMA address (IOVA) window size in bytes */
	hv_u64 phys_base; /**< Physical address (PA) base for translation */
	hv_u32 partition; /**< Partition ID owning this DMA window */
} hv_smmu_window_t;

/**
 * Device DMA configuration.
 */
typedef struct {
	hv_u16 streamid; /**< Device StreamID */
	hv_u32 partition; /**< Partition owning this device */
	hv_dma_access_t access; /**< DMA access rights (RO/WO/RW) */
	hv_u64 dma_base; /**< Assigned DMA window base (IOVA) */
	hv_u64 dma_size; /**< Assigned DMA window size */
} hv_device_dma_t;

/**
 * Initialize SMMU subsystem.
 *
 * Must be called once at hypervisor startup before any device assignment.
 * Initializes internal DMA translation tables and StreamID allocation state.
 *
 * @return HV_OK on success, HV_ENOTSUP if SMMU unavailable on this platform
 */
hv_status_t hv_smmu_init(void);

/**
 * Allocate StreamID for a device.
 *
 * Reserves a StreamID for a new device, assigning it to a partition.
 * Each StreamID maps to a unique device in the system.
 *
 * @param partition    Partition ID to own this device
 * @param streamid     Output: allocated StreamID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition invalid or streamid NULL
 * @return HV_ENOSPC if max devices reached (HV_MAX_SMMU_DEVICES)
 */
hv_status_t hv_smmu_allocate_streamid(hv_u32 partition, hv_u16 *streamid);

/**
 * Configure DMA window for a device.
 *
 * Sets up IOMMU translation tables to map a device's DMA address space (IOVA)
 * to a physical address range within the partition's assigned memory.
 *
 * @param streamid     Device StreamID (must be allocated)
 * @param dma_base     DMA base address (IOVA) - must be aligned
 * @param dma_size     DMA window size in bytes - must be power-of-2
 * @param phys_base    Physical address base for translation - must be aligned
 * @param access       DMA access rights (RO/WO/RW)
 *
 * @return HV_OK on success
 * @return HV_EINVAL if addresses misaligned or size not power-of-2
 * @return HV_EPERM if streamid not allocated
 * @return HV_ENOSPC if DMA window overlaps existing device windows
 *
 * Note: DMA window must be within partition's stage-2 memory allocation.
 */
hv_status_t hv_smmu_configure_dma_window(hv_u16 streamid, hv_u64 dma_base,
					 hv_u64 dma_size, hv_u64 phys_base,
					 hv_dma_access_t access);

/**
 * Revoke DMA access for a device.
 *
 * Removes device's DMA window configuration and frees associated resources.
 * After this call, device cannot perform DMA transactions.
 *
 * @param streamid     Device StreamID to revoke
 *
 * @return HV_OK on success
 * @return HV_EINVAL if streamid invalid
 * @return HV_EPERM if already revoked or not allocated
 */
hv_status_t hv_smmu_revoke_dma_access(hv_u16 streamid);

/**
 * Query device DMA configuration.
 *
 * Retrieves current DMA configuration for a device.
 *
 * @param streamid     Device StreamID
 * @param dma_info     Output: DMA configuration
 *
 * @return HV_OK on success
 * @return HV_EINVAL if streamid invalid or dma_info NULL
 * @return HV_EPERM if streamid not allocated
 */
hv_status_t hv_smmu_get_device_dma(hv_u16 streamid, hv_device_dma_t *dma_info);

/**
 * Check if device has DMA access to address range.
 *
 * Validates whether a device (StreamID) is authorized to perform DMA
 * to the specified address range within its configured DMA window.
 *
 * @param streamid     Device StreamID
 * @param dma_addr     DMA address (IOVA) to check
 * @param size         Address range size in bytes
 * @param access       Required access type (RO/WO/RW)
 *
 * @return HV_OK if access permitted
 * @return HV_EINVAL if address range invalid
 * @return HV_EPERM if device lacks required access or address outside DMA window
 */
hv_status_t hv_smmu_check_dma_access(hv_u16 streamid, hv_u64 dma_addr,
				     hv_u64 size, hv_dma_access_t access);

/**
 * Reset all DMA configuration for a partition.
 *
 * Revokes DMA access for all devices assigned to a partition.
 * Used during partition teardown or recovery.
 *
 * @param partition    Partition ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition invalid
 */
hv_status_t hv_smmu_reset_partition(hv_u32 partition);

#ifdef __cplusplus
}
#endif

#endif /* HAVEN_SMMU_H */
