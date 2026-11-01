/**
 * SMMU implementation for DMA isolation enforcement.
 *
 * @file src/core/dma/smmu.c
 */

#include <haven/smmu.h>
#include <haven/stage2.h>
#include <haven/string.h>
#include <haven/types.h>
#ifdef HAVEN_ARCH_ARM64
#include "drivers/iommu/smmu_v3.h"
/* Per-stream DMA translation tables, arch/arm64/mm.c */
extern hv_status_t hv_arch_dma_context_map(uint32_t sid, uint64_t iova,
					   uint64_t pa, uint64_t size,
					   uint32_t access, uint32_t vmid,
					   uint64_t *vttbr);
extern hv_status_t hv_arch_dma_context_unmap(uint32_t sid, uint64_t iova,
					     uint64_t size);
#endif

/**
 * Internal state tracking for SMMU.  Indexed by hardware StreamID.
 */
static struct {
	hv_u16 streamid_to_partition[HV_MAX_SMMU_STREAMS];
	hv_u16 streamid_count;
	int streamid_allocated[HV_MAX_SMMU_STREAMS];
	int dma_vmid_used[HV_MAX_SMMU_DEVICES];
} smmu_state = {
	.streamid_count = 0,
	.streamid_allocated = {0},
	.streamid_to_partition = {0},
};

/**
 * Device DMA configuration tracking.
 */
static struct {
	hv_u16 streamid;
	hv_u32 partition;
	hv_dma_access_t access;
	hv_u64 dma_base;
	hv_u64 dma_size;
	hv_u64 phys_base;
	hv_u32 vmid; /* SMMU TLB tag of this window */
	int configured;
} device_dma_config[HV_MAX_SMMU_STREAMS] = {0};

/**
 * Check if two DMA windows overlap.
 *
 * Returns 1 if windows [base1, base1+size1) and [base2, base2+size2) overlap, 0
 * otherwise.
 */
static int dma_windows_overlap(hv_u64 base1, hv_u64 size1, hv_u64 base2,
			       hv_u64 size2)
{
	hv_u64 end1 = base1 + size1;
	hv_u64 end2 = base2 + size2;

	if (base1 >= end2 || base2 >= end1) {
		return 0;
	}
	return 1;
}

/**
 * Check if address is aligned to power-of-2 boundary.
 */
static int is_power_of_two(hv_u64 size)
{
	return (size != 0) && ((size & (size - 1)) == 0);
}

static int is_valid_dma_access(hv_dma_access_t access)
{
	return access == HV_DMA_RO || access == HV_DMA_WO ||
	       access == HV_DMA_RW;
}

hv_status_t hv_smmu_init(void)
{
	/**
   * Initialize SMMU state.
   * In real implementation, would check hardware availability and
   * configure translation table roots.
   */
	memset(&smmu_state, 0, sizeof(smmu_state));
	memset(&device_dma_config, 0, sizeof(device_dma_config));
	return HV_OK;
}

hv_status_t hv_smmu_allocate_streamid(hv_u32 partition, hv_u16 *streamid)
{
	if (streamid == NULL) {
		return HV_EINVAL;
	}

	if (partition >= HV_MAX_SMMU_PARTITIONS) {
		return HV_EINVAL;
	}

	/* Check if we've reached max devices. */
	if (smmu_state.streamid_count >= HV_MAX_SMMU_DEVICES) {
		return HV_ENOSPC;
	}

	/* Find first unallocated StreamID. */
	for (hv_u16 i = 0; i < HV_MAX_SMMU_DEVICES; i++) {
		if (!smmu_state.streamid_allocated[i]) {
			smmu_state.streamid_allocated[i] = 1;
			smmu_state.streamid_to_partition[i] = partition;
			smmu_state.streamid_count++;
			*streamid = i;
			return HV_OK;
		}
	}

	return HV_ENOSPC;
}

hv_status_t hv_smmu_assign_streamid(hv_u32 partition, hv_u16 hw_streamid)
{
	if (partition >= HV_MAX_SMMU_PARTITIONS ||
	    hw_streamid >= HV_MAX_SMMU_STREAMS) {
		return HV_EINVAL;
	}

	if (smmu_state.streamid_allocated[hw_streamid]) {
		return HV_EPERM;
	}

	if (smmu_state.streamid_count >= HV_MAX_SMMU_DEVICES) {
		return HV_ENOSPC;
	}

	smmu_state.streamid_allocated[hw_streamid] = 1;
	smmu_state.streamid_to_partition[hw_streamid] = (hv_u16)partition;
	smmu_state.streamid_count++;
	return HV_OK;
}

/* Take a free DMA VMID slot; returns the VMID, or 0 if none is free. */
static hv_u32 dma_vmid_take(void)
{
	for (hv_u32 i = 0; i < HV_MAX_SMMU_DEVICES; i++) {
		if (!smmu_state.dma_vmid_used[i]) {
			smmu_state.dma_vmid_used[i] = 1;
			return HV_SMMU_DMA_VMID_BASE + i;
		}
	}
	return 0;
}

static void dma_vmid_put(hv_u32 vmid)
{
	if (vmid >= HV_SMMU_DMA_VMID_BASE &&
	    vmid < HV_SMMU_DMA_VMID_BASE + HV_MAX_SMMU_DEVICES) {
		smmu_state.dma_vmid_used[vmid - HV_SMMU_DMA_VMID_BASE] = 0;
	}
}

hv_status_t hv_smmu_configure_dma_window(hv_u16 streamid, hv_u64 dma_base,
					 hv_u64 dma_size, hv_u64 phys_base,
					 hv_dma_access_t access)
{
	if (!is_valid_dma_access(access)) {
		return HV_EINVAL;
	}

	if (streamid >= HV_MAX_SMMU_STREAMS) {
		return HV_EINVAL;
	}

	/* Check if StreamID is allocated. */
	if (!smmu_state.streamid_allocated[streamid]) {
		return HV_EPERM;
	}

	if (device_dma_config[streamid].configured) {
		return HV_EPERM;
	}

	/* Validate alignment. */
	if ((dma_base & 0xFFF) != 0 || (phys_base & 0xFFF) != 0) {
		return HV_EINVAL; /* 4K page alignment required */
	}

	/* Validate size is power-of-2. */
	if (!is_power_of_two(dma_size)) {
		return HV_EINVAL;
	}

	if (hv_stage2_partition_contains_pa(
		    smmu_state.streamid_to_partition[streamid], phys_base,
		    dma_size) != HV_OK) {
		return HV_EPERM;
	}

	/* Check for DMA window overlap with other devices. */
	for (hv_u16 i = 0; i < HV_MAX_SMMU_STREAMS; i++) {
		if (i == streamid || !device_dma_config[i].configured) {
			continue;
		}

		if (dma_windows_overlap(dma_base, dma_size,
					device_dma_config[i].dma_base,
					device_dma_config[i].dma_size)) {
			return HV_ENOSPC; /* Window overlap detected */
		}
	}

	hv_u32 vmid = dma_vmid_take();
	if (vmid == 0) {
		return HV_ENOSPC;
	}

#ifdef HAVEN_ARCH_ARM64
	/* Bind hardware before committing state: build a stage-2 table for
	 * this stream that maps exactly [dma_base, dma_base + dma_size) to
	 * [phys_base, ...) with the granted access, then point the stream's
	 * STE at it.  The device reaches nothing else, not even the rest of
	 * its partition. */
	{
		uint64_t vttbr;
		hv_status_t rc = hv_arch_dma_context_map(streamid, dma_base,
							 phys_base, dma_size,
							 (uint32_t)access, vmid,
							 &vttbr);
		if (rc == HV_OK) {
			rc = smmu_v3_set_ste_partition(streamid, vttbr, vmid);
		}
		if (rc != HV_OK) {
			(void)smmu_v3_set_ste_abort(streamid);
			(void)hv_arch_dma_context_unmap(streamid, dma_base,
							dma_size);
			dma_vmid_put(vmid);
			return rc;
		}
	}
#endif

	/* Store configuration. */
	device_dma_config[streamid].streamid = streamid;
	device_dma_config[streamid].partition =
		smmu_state.streamid_to_partition[streamid];
	device_dma_config[streamid].access = access;
	device_dma_config[streamid].dma_base = dma_base;
	device_dma_config[streamid].dma_size = dma_size;
	device_dma_config[streamid].phys_base = phys_base;
	device_dma_config[streamid].vmid = vmid;
	device_dma_config[streamid].configured = 1;

	return HV_OK;
}

hv_status_t hv_smmu_revoke_dma_access(hv_u16 streamid)
{
	hv_status_t rc = HV_OK;

	if (streamid >= HV_MAX_SMMU_STREAMS) {
		return HV_EINVAL;
	}

	if (!smmu_state.streamid_allocated[streamid]) {
		return HV_EPERM;
	}

	if (!device_dma_config[streamid].configured) {
		return HV_EPERM; /* Already revoked or never configured */
	}

#ifdef HAVEN_ARCH_ARM64
	/* Deny first, then take the window's mappings away.  HV_EIO means the
	 * SMMU did not confirm the invalidation; ownership is still revoked. */
	rc = smmu_v3_set_ste_abort(streamid);
	(void)hv_arch_dma_context_unmap(streamid,
					device_dma_config[streamid].dma_base,
					device_dma_config[streamid].dma_size);
#endif
	dma_vmid_put(device_dma_config[streamid].vmid);

	/* Clear configuration. */
	memset(&device_dma_config[streamid], 0, sizeof(device_dma_config[0]));
	smmu_state.streamid_allocated[streamid] = 0;
	smmu_state.streamid_to_partition[streamid] = 0U;
	if (smmu_state.streamid_count != 0U) {
		smmu_state.streamid_count--;
	}

	return rc;
}

hv_status_t hv_smmu_get_device_dma(hv_u16 streamid, hv_device_dma_t *dma_info)
{
	if (dma_info == NULL) {
		return HV_EINVAL;
	}

	if (streamid >= HV_MAX_SMMU_STREAMS) {
		return HV_EINVAL;
	}

	if (!smmu_state.streamid_allocated[streamid]) {
		return HV_EPERM;
	}

	if (!device_dma_config[streamid].configured) {
		return HV_EPERM;
	}

	dma_info->streamid = streamid;
	dma_info->partition = device_dma_config[streamid].partition;
	dma_info->access = device_dma_config[streamid].access;
	dma_info->dma_base = device_dma_config[streamid].dma_base;
	dma_info->dma_size = device_dma_config[streamid].dma_size;

	return HV_OK;
}

hv_status_t hv_smmu_check_dma_access(hv_u16 streamid, hv_u64 dma_addr,
				     hv_u64 size, hv_dma_access_t access)
{
	if (streamid >= HV_MAX_SMMU_STREAMS) {
		return HV_EINVAL;
	}

	if (!smmu_state.streamid_allocated[streamid]) {
		return HV_EPERM;
	}

	if (!device_dma_config[streamid].configured) {
		return HV_EPERM;
	}

	if (!is_valid_dma_access(access)) {
		return HV_EINVAL;
	}

	/* Check if requested address range is within DMA window. */
	hv_u64 window_end = device_dma_config[streamid].dma_base +
			    device_dma_config[streamid].dma_size;
	hv_u64 request_end = dma_addr + size;

	if (dma_addr < device_dma_config[streamid].dma_base ||
	    request_end > window_end) {
		return HV_EPERM; /* Outside DMA window */
	}

	/* Check access rights. */
	hv_dma_access_t allowed = device_dma_config[streamid].access;
	if ((access & allowed) != access) {
		return HV_EPERM; /* Insufficient access rights */
	}

	return HV_OK;
}

hv_status_t hv_smmu_reset_partition(hv_u32 partition)
{
	if (partition >= HV_MAX_SMMU_PARTITIONS) {
		return HV_EINVAL;
	}

	/* Free every StreamID owned by this partition, whether or not its DMA
	 * window was ever configured.  An allocated-but-unconfigured StreamID
	 * (e.g., one whose configure call was rejected due to a PA overlap)
	 * must not persist after a partition reset - it would otherwise leak
	 * quota from the global StreamID pool. */
	for (hv_u16 i = 0; i < HV_MAX_SMMU_STREAMS; i++) {
		if (!smmu_state.streamid_allocated[i] ||
		    smmu_state.streamid_to_partition[i] != partition) {
			continue;
		}
		if (device_dma_config[i].configured) {
			/* Configured: revoke tears down hardware STE + state. */
			hv_smmu_revoke_dma_access(i);
		} else {
			/* Allocated but unconfigured: free pool slot only. */
			smmu_state.streamid_allocated[i] = 0;
			smmu_state.streamid_to_partition[i] = 0U;
			if (smmu_state.streamid_count != 0U) {
				smmu_state.streamid_count--;
			}
		}
	}

	return HV_OK;
}
