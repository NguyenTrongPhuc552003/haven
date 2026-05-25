/**
 * SMMU hardware integration test - DMA policy enforcement scenarios.
 *
 * Validates Stage-2 + SMMU coherence for the hardware integration scenario.
 * Each scenario tests a distinct isolation boundary:
 *
 *   S1  Unassigned StreamID is denied by hv_smmu_check_dma_access.
 *   S2  DMA outside the assigned 4 MB window is denied.
 *   S3  Cross-partition DMA window overlap is rejected by configure.
 *   S4  hv_smmu_reset_partition revokes all StreamIDs for that partition.
 *   S5  Revocation + reuse: StreamID can be reallocated to a different
 *       partition after revocation.
 *
 * Unlike assert()-based tests, each scenario reports [PASS]/[FAIL] to
 * stderr and exits with status 1 on the first failure so CI can pinpoint
 * the failing scenario without a signal-based crash.
 *
 * @file tests/integration/test_smmu_hardware.c
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <haven/smmu.h>
#include <haven/stage2.h>

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

#define FAIL_IF(cond, msg)                                     \
	do {                                                   \
		if (cond) {                                    \
			fprintf(stderr, "[FAIL] %s\n", (msg)); \
			exit(1);                               \
		}                                              \
	} while (0)

#define PASS(msg) fprintf(stderr, "[PASS] %s\n", (msg))

/* -----------------------------------------------------------------------
 * Partition memory layout used across scenarios
 *
 *   Partition A: PA 0x80000000 .. 0x83FFFFFF (64 MiB)
 *   Partition B: PA 0x90000000 .. 0x93FFFFFF (64 MiB)
 *
 * The DMA window for scenario S2 is 4 MiB at the start of partition A.
 * ----------------------------------------------------------------------- */

#define PA_A_BASE 0x80000000ULL
#define PA_A_SIZE 0x04000000ULL /* 64 MiB */
#define PA_B_BASE 0x90000000ULL
#define PA_B_SIZE 0x04000000ULL /* 64 MiB */

#define DMA_WINDOW_BASE PA_A_BASE
#define DMA_WINDOW_SIZE 0x00400000ULL /* 4 MiB */

#define PART_A 1U
#define PART_B 2U

/* -----------------------------------------------------------------------
 * S1 - Unassigned StreamID is denied
 *
 * A StreamID value that was never allocated must be rejected by
 * hv_smmu_check_dma_access with HV_EPERM.
 * ----------------------------------------------------------------------- */

static void scenario_s1_unassigned_streamid_denied(void)
{
	/*
   * StreamID 63 is within the valid range (< HV_MAX_SMMU_DEVICES = 64)
   * but was never allocated.  hv_smmu_check_dma_access must return
   * HV_EPERM for any unallocated StreamID, enforcing deny-by-default.
   */
	hv_u16 phantom_sid = 63;
	hv_status_t rc;

	rc = hv_smmu_check_dma_access(phantom_sid, 0x80000000ULL, 0x1000U,
				      HV_DMA_RW);
	FAIL_IF(rc != HV_EPERM,
		"S1: unassigned StreamID should return HV_EPERM");

	PASS("S1: unassigned StreamID is denied");
}

/* -----------------------------------------------------------------------
 * S2 - DMA outside the assigned 4 MiB window is denied
 * ----------------------------------------------------------------------- */

static void scenario_s2_dma_outside_window_denied(void)
{
	hv_u16 sid = 0;
	hv_status_t rc;

	/* Allocate and configure a 4 MiB DMA window for partition A. */
	rc = hv_smmu_allocate_streamid(PART_A, &sid);
	FAIL_IF(rc != HV_OK, "S2: allocate StreamID for partition A");

	rc = hv_smmu_configure_dma_window(sid, DMA_WINDOW_BASE, DMA_WINDOW_SIZE,
					  DMA_WINDOW_BASE, HV_DMA_RW);
	FAIL_IF(rc != HV_OK, "S2: configure 4 MiB DMA window");

	/* Access inside the window must be permitted. */
	rc = hv_smmu_check_dma_access(sid, DMA_WINDOW_BASE + 0x1000U, 0x1000U,
				      HV_DMA_RW);
	FAIL_IF(rc != HV_OK, "S2: access inside window should be HV_OK");

	/* Access exactly at the window end boundary must be denied. */
	rc = hv_smmu_check_dma_access(sid, DMA_WINDOW_BASE + DMA_WINDOW_SIZE,
				      0x1000U, HV_DMA_RW);
	FAIL_IF(rc != HV_EPERM,
		"S2: access at window end should return HV_EPERM");

	/* Access well outside the window must be denied. */
	rc = hv_smmu_check_dma_access(
		sid, DMA_WINDOW_BASE + DMA_WINDOW_SIZE + 0x10000U, 0x1000U,
		HV_DMA_RW);
	FAIL_IF(rc != HV_EPERM,
		"S2: access beyond window should return HV_EPERM");

	/* Access before the window base must be denied. */
	rc = hv_smmu_check_dma_access(sid, DMA_WINDOW_BASE - 0x1000U, 0x1000U,
				      HV_DMA_RW);
	FAIL_IF(rc != HV_EPERM,
		"S2: access before window base should return HV_EPERM");

	/* Clean up. */
	rc = hv_smmu_revoke_dma_access(sid);
	FAIL_IF(rc != HV_OK, "S2: revoke DMA access");

	PASS("S2: DMA outside assigned window is denied");
}

/* -----------------------------------------------------------------------
 * S3 - Cross-partition DMA window overlap is rejected
 *
 * After partition A has a DMA window backed by PA_A, a configure call
 * on a partition B StreamID that targets the same physical range must
 * return HV_EPERM (the stage-2 backing check prevents the overlap).
 * ----------------------------------------------------------------------- */

static void scenario_s3_cross_partition_overlap_denied(void)
{
	hv_u16 sid_a = 0;
	hv_u16 sid_b = 0;
	hv_status_t rc;

	/* Establish a DMA window for partition A. */
	rc = hv_smmu_allocate_streamid(PART_A, &sid_a);
	FAIL_IF(rc != HV_OK, "S3: allocate StreamID for partition A");

	rc = hv_smmu_configure_dma_window(sid_a, DMA_WINDOW_BASE,
					  DMA_WINDOW_SIZE, DMA_WINDOW_BASE,
					  HV_DMA_RW);
	FAIL_IF(rc != HV_OK, "S3: configure DMA window for partition A");

	/* Allocate a StreamID for partition B. */
	rc = hv_smmu_allocate_streamid(PART_B, &sid_b);
	FAIL_IF(rc != HV_OK, "S3: allocate StreamID for partition B");

	/*
   * Attempt to map partition B's StreamID to PA_A_BASE - partition A's
   * physical range.  The DMA window phys_base overlaps a region owned
   * by partition A; this must be rejected.
   */
	rc = hv_smmu_configure_dma_window(sid_b,
					  PA_B_BASE, /* IOVA in B's space */
					  DMA_WINDOW_SIZE,
					  DMA_WINDOW_BASE, /* PA in A's range */
					  HV_DMA_RW);
	FAIL_IF(rc != HV_EPERM,
		"S3: cross-partition PA overlap should return HV_EPERM");

	/* Clean up - revoke sid_a's configured window; free sid_b which was
	 * allocated but never configured (reset_partition now handles both). */
	rc = hv_smmu_revoke_dma_access(sid_a);
	FAIL_IF(rc != HV_OK, "S3: revoke partition A DMA access");

	/* sid_b was allocated but its configure call was rejected.  Use
	 * reset_partition so the unconfigured slot is freed. */
	rc = hv_smmu_reset_partition(PART_B);
	FAIL_IF(rc != HV_OK,
		"S3: reset partition B to free unconfigured StreamID");

	PASS("S3: cross-partition DMA window overlap is rejected");
}

/* -----------------------------------------------------------------------
 * S4 - hv_smmu_reset_partition revokes all StreamIDs for that partition
 *
 * Configure two StreamIDs for partition A; reset the partition; verify
 * both StreamIDs are subsequently inaccessible.
 * ----------------------------------------------------------------------- */

static void scenario_s4_reset_partition_revokes_all(void)
{
	hv_u16 sid1 = 0;
	hv_u16 sid2 = 0;
	hv_status_t rc;

	/* Allocate first StreamID. */
	rc = hv_smmu_allocate_streamid(PART_A, &sid1);
	FAIL_IF(rc != HV_OK, "S4: allocate first StreamID for partition A");

	rc = hv_smmu_configure_dma_window(sid1, DMA_WINDOW_BASE,
					  DMA_WINDOW_SIZE, DMA_WINDOW_BASE,
					  HV_DMA_RO);
	FAIL_IF(rc != HV_OK, "S4: configure first DMA window");

	/* Allocate second StreamID in the remaining PA space. */
	rc = hv_smmu_allocate_streamid(PART_A, &sid2);
	FAIL_IF(rc != HV_OK, "S4: allocate second StreamID for partition A");

	rc = hv_smmu_configure_dma_window(
		sid2, DMA_WINDOW_BASE + DMA_WINDOW_SIZE, DMA_WINDOW_SIZE,
		DMA_WINDOW_BASE + DMA_WINDOW_SIZE, HV_DMA_RO);
	FAIL_IF(rc != HV_OK, "S4: configure second DMA window");

	/* Verify both are accessible before reset. */
	rc = hv_smmu_check_dma_access(sid1, DMA_WINDOW_BASE, 0x1000U,
				      HV_DMA_RO);
	FAIL_IF(rc != HV_OK, "S4: sid1 should be accessible before reset");

	rc = hv_smmu_check_dma_access(sid2, DMA_WINDOW_BASE + DMA_WINDOW_SIZE,
				      0x1000U, HV_DMA_RO);
	FAIL_IF(rc != HV_OK, "S4: sid2 should be accessible before reset");

	/* Reset partition A - must revoke all its StreamIDs. */
	rc = hv_smmu_reset_partition(PART_A);
	FAIL_IF(rc != HV_OK, "S4: hv_smmu_reset_partition should succeed");

	/* Both StreamIDs must now be inaccessible. */
	rc = hv_smmu_check_dma_access(sid1, DMA_WINDOW_BASE, 0x1000U,
				      HV_DMA_RO);
	FAIL_IF(rc != HV_EPERM,
		"S4: sid1 should be denied after partition reset");

	rc = hv_smmu_check_dma_access(sid2, DMA_WINDOW_BASE + DMA_WINDOW_SIZE,
				      0x1000U, HV_DMA_RO);
	FAIL_IF(rc != HV_EPERM,
		"S4: sid2 should be denied after partition reset");

	PASS("S4: hv_smmu_reset_partition revokes all StreamIDs");
}

/* -----------------------------------------------------------------------
 * S5 - Revocation + reuse: StreamID can be reallocated to a different
 * partition after revocation.
 * ----------------------------------------------------------------------- */

static void scenario_s5_revocation_and_reuse(void)
{
	hv_u16 sid = 0;
	hv_status_t rc;

	/* Allocate and configure a window for partition A. */
	rc = hv_smmu_allocate_streamid(PART_A, &sid);
	FAIL_IF(rc != HV_OK, "S5: allocate StreamID for partition A");

	rc = hv_smmu_configure_dma_window(sid, DMA_WINDOW_BASE, DMA_WINDOW_SIZE,
					  DMA_WINDOW_BASE, HV_DMA_WO);
	FAIL_IF(rc != HV_OK, "S5: configure DMA window for partition A");

	/* Revoke. */
	rc = hv_smmu_revoke_dma_access(sid);
	FAIL_IF(rc != HV_OK, "S5: revoke DMA access");

	/* After revocation the StreamID must be inaccessible. */
	rc = hv_smmu_check_dma_access(sid, DMA_WINDOW_BASE, 0x1000U, HV_DMA_WO);
	FAIL_IF(rc != HV_EPERM,
		"S5: StreamID should be denied after revocation");

	/* Re-allocate the freed slot to partition B. */
	hv_u16 new_sid = 0;
	rc = hv_smmu_allocate_streamid(PART_B, &new_sid);
	FAIL_IF(rc != HV_OK, "S5: reallocation to partition B should succeed");

	rc = hv_smmu_configure_dma_window(new_sid, PA_B_BASE, DMA_WINDOW_SIZE,
					  PA_B_BASE, HV_DMA_RW);
	FAIL_IF(rc != HV_OK,
		"S5: configure reallocated StreamID for partition B");

	rc = hv_smmu_check_dma_access(new_sid, PA_B_BASE, 0x1000U, HV_DMA_RW);
	FAIL_IF(rc != HV_OK,
		"S5: reallocated StreamID should be accessible for partition B");

	/* Clean up. */
	rc = hv_smmu_revoke_dma_access(new_sid);
	FAIL_IF(rc != HV_OK, "S5: final revocation");

	PASS("S5: revocation and reuse to different partition succeeds");
}

/* -----------------------------------------------------------------------
 * S6 - Read-only DMA window blocks write access
 *
 * Configure a window with HV_DMA_RO; a write-access check must return
 * HV_EPERM, confirming DMA access-mode isolation is enforced.
 * ----------------------------------------------------------------------- */

static void scenario_s6_readonly_window_blocks_write(void)
{
	hv_u16 sid = 0;
	hv_status_t rc;

	rc = hv_smmu_allocate_streamid(PART_A, &sid);
	FAIL_IF(rc != HV_OK, "S6: allocate StreamID");

	rc = hv_smmu_configure_dma_window(sid, DMA_WINDOW_BASE, DMA_WINDOW_SIZE,
					  DMA_WINDOW_BASE, HV_DMA_RO);
	FAIL_IF(rc != HV_OK, "S6: configure read-only DMA window");

	/* Read access within window must be permitted. */
	rc = hv_smmu_check_dma_access(sid, DMA_WINDOW_BASE + 0x1000U, 0x1000U,
				      HV_DMA_RO);
	FAIL_IF(rc != HV_OK, "S6: read within RO window should be HV_OK");

	/* Write access must be denied even though the address is in window. */
	rc = hv_smmu_check_dma_access(sid, DMA_WINDOW_BASE + 0x1000U, 0x1000U,
				      HV_DMA_WO);
	FAIL_IF(rc != HV_EPERM,
		"S6: write to RO window should return HV_EPERM");

	/* Read-write access must also be denied. */
	rc = hv_smmu_check_dma_access(sid, DMA_WINDOW_BASE + 0x1000U, 0x1000U,
				      HV_DMA_RW);
	FAIL_IF(rc != HV_EPERM, "S6: RW to RO window should return HV_EPERM");

	rc = hv_smmu_revoke_dma_access(sid);
	FAIL_IF(rc != HV_OK, "S6: revoke");

	PASS("S6: read-only window blocks write DMA access");
}

/* -----------------------------------------------------------------------
 * S7 - Write-only DMA window blocks read access
 *
 * Symmetric to S6: a HV_DMA_WO window must deny HV_DMA_RO checks.
 * ----------------------------------------------------------------------- */

static void scenario_s7_writeonly_window_blocks_read(void)
{
	hv_u16 sid = 0;
	hv_status_t rc;

	rc = hv_smmu_allocate_streamid(PART_B, &sid);
	FAIL_IF(rc != HV_OK, "S7: allocate StreamID");

	rc = hv_smmu_configure_dma_window(sid, PA_B_BASE, DMA_WINDOW_SIZE,
					  PA_B_BASE, HV_DMA_WO);
	FAIL_IF(rc != HV_OK, "S7: configure write-only DMA window");

	/* Write access within window must be permitted. */
	rc = hv_smmu_check_dma_access(sid, PA_B_BASE + 0x1000U, 0x1000U,
				      HV_DMA_WO);
	FAIL_IF(rc != HV_OK, "S7: write within WO window should be HV_OK");

	/* Read access must be denied. */
	rc = hv_smmu_check_dma_access(sid, PA_B_BASE + 0x1000U, 0x1000U,
				      HV_DMA_RO);
	FAIL_IF(rc != HV_EPERM,
		"S7: read from WO window should return HV_EPERM");

	/* Read-write access must also be denied. */
	rc = hv_smmu_check_dma_access(sid, PA_B_BASE + 0x1000U, 0x1000U,
				      HV_DMA_RW);
	FAIL_IF(rc != HV_EPERM, "S7: RW from WO window should return HV_EPERM");

	rc = hv_smmu_revoke_dma_access(sid);
	FAIL_IF(rc != HV_OK, "S7: revoke");

	PASS("S7: write-only window blocks read DMA access");
}

/* -----------------------------------------------------------------------
 * S8 - StreamID pool exhaustion returns HV_ENOSPC
 *
 * Allocate exactly HV_MAX_SMMU_DEVICES StreamIDs; the (N+1)-th call
 * must return HV_ENOSPC (not crash or wrap).  Confirms the hypervisor
 * enforces a hard upper bound on concurrent DMA assignments, preventing
 * a rogue partition from consuming unbounded SMMU resources.
 * ----------------------------------------------------------------------- */

static void scenario_s8_streamid_exhaustion(void)
{
	hv_u16 sid = 0;
	hv_status_t rc;
	int i;

	/* Fill the pool up to the maximum. */
	for (i = 0; i < HV_MAX_SMMU_DEVICES; i++) {
		rc = hv_smmu_allocate_streamid(PART_A, &sid);
		FAIL_IF(rc != HV_OK, "S8: pool fill should succeed");
	}

	/* One more allocation must fail with HV_ENOSPC. */
	rc = hv_smmu_allocate_streamid(PART_A, &sid);
	FAIL_IF(rc != HV_ENOSPC,
		"S8: allocation beyond HV_MAX_SMMU_DEVICES should return HV_ENOSPC");

	/* Reset to clean state for any subsequent tests. */
	rc = hv_smmu_reset_partition(PART_A);
	FAIL_IF(rc != HV_OK, "S8: reset after exhaustion test");

	PASS("S8: StreamID pool exhaustion returns HV_ENOSPC");
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int main(void)
{
	hv_status_t rc;

	/* Bootstrap required subsystems. */
	rc = hv_stage2_init();
	FAIL_IF(rc != HV_OK, "stage2 init");

	rc = hv_smmu_init();
	FAIL_IF(rc != HV_OK, "smmu init");

	/* Map partition memory regions so the SMMU policy layer can verify
   * phys_base ownership against stage-2 tables. */
	struct hv_mem_region region_a = {
		.ipa_base = PA_A_BASE,
		.pa_base = PA_A_BASE,
		.size = PA_A_SIZE,
		.attrs = 0,
	};
	struct hv_partition_mem part_a_cfg = {
		.partition_id = PART_A,
		.regions = &region_a,
		.region_count = 1,
	};

	struct hv_mem_region region_b = {
		.ipa_base = PA_B_BASE,
		.pa_base = PA_B_BASE,
		.size = PA_B_SIZE,
		.attrs = 0,
	};
	struct hv_partition_mem part_b_cfg = {
		.partition_id = PART_B,
		.regions = &region_b,
		.region_count = 1,
	};

	rc = hv_stage2_map_partition(&part_a_cfg);
	FAIL_IF(rc != HV_OK, "stage2 map partition A");

	rc = hv_stage2_map_partition(&part_b_cfg);
	FAIL_IF(rc != HV_OK, "stage2 map partition B");

	/* Run scenarios. */
	scenario_s1_unassigned_streamid_denied();
	scenario_s2_dma_outside_window_denied();
	scenario_s3_cross_partition_overlap_denied();
	scenario_s4_reset_partition_revokes_all();
	scenario_s5_revocation_and_reuse();
	scenario_s6_readonly_window_blocks_write();
	scenario_s7_writeonly_window_blocks_read();
	scenario_s8_streamid_exhaustion();

	fprintf(stderr, "[test_smmu_hardware] all scenarios passed\n");
	return 0;
}