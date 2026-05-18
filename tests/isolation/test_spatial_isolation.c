/**
 * Spatial isolation boundary tests - Chapter 4 evidence.
 *
 * Exercises all three spatial isolation enforcement layers together:
 *   - Stage-2 memory mapping (partition PA boundaries)
 *   - IOMMU group ownership (device assignment policy)
 *   - SMMU DMA window (hardware-enforced DMA containment)
 *
 * Each scenario documents the precise violation that is denied and the
 * expected status code, matching the thesis acceptance criterion:
 *   "Guest cannot map or access non-owned memory regions."
 *   "DMA from non-owner partition is blocked by policy."
 *
 * @file tests/isolation/test_spatial_isolation.c
 */

#include <assert.h>
#include <haven/iommu.h>
#include <haven/smmu.h>
#include <haven/stage2.h>
#include <stdio.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg)                      \
	do {                                \
		printf("[FAIL] %s\n", msg); \
		assert(0);                  \
	} while (0)

/* -----------------------------------------------------------------------
 * Partition layout used across all scenarios:
 *
 *   Partition 1: PA 0x10000000 .. 0x11000000  (16 MiB)
 *   Partition 2: PA 0x20000000 .. 0x21000000  (16 MiB)
 *   IOMMU group 1 → partition 1
 *   IOMMU group 2 → partition 2
 * ----------------------------------------------------------------------- */

static struct hv_mem_region region_p1 = {
	.ipa_base = 0x10000000,
	.pa_base = 0x10000000,
	.size = 0x01000000,
	.attrs = 0,
};

static struct hv_mem_region region_p2 = {
	.ipa_base = 0x20000000,
	.pa_base = 0x20000000,
	.size = 0x01000000,
	.attrs = 0,
};

static struct hv_partition_mem cfg_p1 = {
	.partition_id = 1,
	.regions = &region_p1,
	.region_count = 1,
};

static struct hv_partition_mem cfg_p2 = {
	.partition_id = 2,
	.regions = &region_p2,
	.region_count = 1,
};

/* -----------------------------------------------------------------------
 * Scenario 1: Stage-2 cross-partition memory conflict
 *
 * A second partition must not be able to map physical memory already
 * owned by another partition. Both same-IPA and overlapping-PA attempts
 * must be denied.
 * ----------------------------------------------------------------------- */
static void test_stage2_cross_partition_boundary(void)
{
	assert(hv_stage2_init() == HV_OK);
	assert(hv_stage2_map_partition(&cfg_p1) == HV_OK);
	assert(hv_stage2_map_partition(&cfg_p2) == HV_OK);

	/* Attempt to map partition 1's PA range as a new partition (3). */
	struct hv_mem_region region_escape = {
		.ipa_base = 0x30000000, /* different IPA */
		.pa_base = 0x10000000, /* but P1's PA - conflict */
		.size = 0x1000,
		.attrs = 0,
	};
	struct hv_partition_mem cfg_escape = {
		.partition_id = 3,
		.regions = &region_escape,
		.region_count = 1,
	};

	hv_status_t s = hv_stage2_map_partition(&cfg_escape);
	assert(s == HV_EPERM);
	TEST_PASS("stage2: cross-partition PA escape denied");

	/* Remapping the same IPA for partition 1 is also denied. */
	s = hv_stage2_map_partition(&cfg_p1);
	assert(s == HV_EPERM);
	TEST_PASS("stage2: double-map of already-owned partition denied");
}

/* -----------------------------------------------------------------------
 * Scenario 2: IOMMU group ownership conflict
 *
 * A partition must not be able to claim an IOMMU group already owned by
 * another partition, even after the API is initialised fresh.
 * ----------------------------------------------------------------------- */
static void test_iommu_group_ownership_boundary(void)
{
	assert(hv_iommu_init() == HV_OK);

	/* Partition 1 claims group 1. */
	assert(hv_iommu_assign_group(1, 1) == HV_OK);
	TEST_PASS("iommu: partition 1 acquires group 1");

	/* Partition 2 attempts to steal group 1. */
	hv_status_t s = hv_iommu_assign_group(1, 2);
	assert(s == HV_EPERM);
	TEST_PASS("iommu: partition 2 cannot steal group 1");

	/* Partition 1 releases; partition 2 can now claim. */
	assert(hv_iommu_release_group(1, 1) == HV_OK);
	assert(hv_iommu_assign_group(1, 2) == HV_OK);
	TEST_PASS("iommu: after release group 1 is reassignable");

	/* Non-owner cannot release. */
	s = hv_iommu_release_group(1, 1);
	assert(s == HV_EPERM);
	TEST_PASS("iommu: non-owner cannot release group");
}

/* -----------------------------------------------------------------------
 * Scenario 3: SMMU DMA window partition-escape denial
 *
 * A device assigned to partition 1 must not be able to configure a DMA
 * window that reaches into partition 2's physical memory.
 * ----------------------------------------------------------------------- */
static void test_smmu_dma_partition_escape(void)
{
	assert(hv_smmu_init() == HV_OK);

	hv_u16 sid1 = 0;
	assert(hv_smmu_allocate_streamid(1, &sid1) == HV_OK);

	/* Configure a DMA window within P1's memory - should succeed. */
	hv_status_t s = hv_smmu_configure_dma_window(sid1, 0x10000000, 0x1000,
						     0x10000000, HV_DMA_RW);
	assert(s == HV_OK);
	TEST_PASS("smmu: DMA window within partition 1 memory accepted");
	assert(hv_smmu_revoke_dma_access(sid1) == HV_OK);

	/* Attempt to configure DMA window into P2's memory from P1's stream. */
	assert(hv_smmu_allocate_streamid(1, &sid1) == HV_OK);
	s = hv_smmu_configure_dma_window(sid1, 0x10000000, 0x1000, 0x20000000,
					 HV_DMA_RW);
	assert(s == HV_EPERM);
	TEST_PASS("smmu: DMA window escaping into partition 2 PA denied");

	/* Attempt to configure a window wholly outside any mapped partition. */
	s = hv_smmu_configure_dma_window(sid1, 0x10000000, 0x1000, 0x50000000,
					 HV_DMA_RW);
	assert(s == HV_EPERM);
	TEST_PASS("smmu: DMA window to unmapped PA region denied");
}

/* -----------------------------------------------------------------------
 * Scenario 4: SMMU access-type boundary on configured window
 *
 * A read-only window must deny write access checks; a write-only window
 * must deny read access checks.
 * ----------------------------------------------------------------------- */
static void test_smmu_access_type_boundary(void)
{
	/* Stage-2 state is already set up by the previous test. */
	assert(hv_smmu_init() == HV_OK);

	hv_u16 sid_ro = 0;
	assert(hv_smmu_allocate_streamid(1, &sid_ro) == HV_OK);
	assert(hv_smmu_configure_dma_window(sid_ro, 0x10000000, 0x1000,
					    0x10000000, HV_DMA_RO) == HV_OK);

	/* Read from RO window - allowed. */
	assert(hv_smmu_check_dma_access(sid_ro, 0x10000000, 0x1000,
					HV_DMA_RO) == HV_OK);
	TEST_PASS("smmu: read access on RO window permitted");

	/* Write to RO window - denied. */
	assert(hv_smmu_check_dma_access(sid_ro, 0x10000000, 0x1000,
					HV_DMA_WO) == HV_EPERM);
	TEST_PASS("smmu: write access on RO window denied");

	hv_u16 sid_wo = 0;
	assert(hv_smmu_allocate_streamid(1, &sid_wo) == HV_OK);
	assert(hv_smmu_configure_dma_window(sid_wo, 0x10001000, 0x1000,
					    0x10001000, HV_DMA_WO) == HV_OK);

	/* Write to WO window - allowed. */
	assert(hv_smmu_check_dma_access(sid_wo, 0x10001000, 0x1000,
					HV_DMA_WO) == HV_OK);
	TEST_PASS("smmu: write access on WO window permitted");

	/* Read from WO window - denied. */
	assert(hv_smmu_check_dma_access(sid_wo, 0x10001000, 0x1000,
					HV_DMA_RO) == HV_EPERM);
	TEST_PASS("smmu: read access on WO window denied");
}

int main(void)
{
	printf("\n=== Spatial Isolation Boundary Tests (Chapter 4 Evidence) ===\n\n");

	test_stage2_cross_partition_boundary();
	test_iommu_group_ownership_boundary();
	test_smmu_dma_partition_escape();
	test_smmu_access_type_boundary();

	printf("\n=== All spatial isolation tests passed ===\n\n");
	return 0;
}
