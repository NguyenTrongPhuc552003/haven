/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/iommu/smmu_v3.h - Public interface for the Haven SMMUv3 driver.
 *
 * Called by src/core/dma/smmu.c after ownership policy validation.
 */

#ifndef HAVEN_DRIVERS_SMMU_V3_H
#define HAVEN_DRIVERS_SMMU_V3_H

#include <stdint.h>
#include <haven/types.h>

/*
 * smmu_v3_init - initialize SMMUv3 with a linear stream table.
 * All STEs default to ABORT (fail-closed).
 *
 * @base:       SMMU MMIO physical base address
 * @nr_streams: Number of StreamIDs to support (power-of-2, max 256)
 */
hv_status_t smmu_v3_init(uintptr_t base, uint32_t nr_streams);

/*
 * smmu_v3_set_ste_partition - enable stage-2 translation for a StreamID.
 * DMA from this device is now gated by the partition's stage-2 table.
 *
 * @sid:   StreamID (must be < nr_streams)
 * @vttbr: VTTBR_EL2 value (stage-2 table base + VMID)
 * @vmid:  Partition VMID
 */
hv_status_t smmu_v3_set_ste_partition(uint32_t sid, uint64_t vttbr, uint32_t vmid);

/*
 * smmu_v3_set_ste_abort - revert StreamID to ABORT (deny all DMA).
 * Used on partition teardown or ownership revocation.
 */
hv_status_t smmu_v3_set_ste_abort(uint32_t sid);

/*
 * smmu_v3_set_ste_bypass - pass-through for a StreamID (no translation).
 * Not used in isolation policy; diagnostic/early-boot use only.
 */
hv_status_t smmu_v3_set_ste_bypass(uint32_t sid);

#endif /* HAVEN_DRIVERS_SMMU_V3_H */
