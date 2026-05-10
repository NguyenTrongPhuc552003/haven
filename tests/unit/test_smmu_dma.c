/**
 * Unit tests for SMMU DMA isolation.
 *
 * Tests cover:
 *   - Device StreamID allocation
 *   - DMA window configuration
 *   - Window overlap detection
 *   - Access permission enforcement
 *   - Partition-based revocation
 *
 * @file tests/unit/test_smmu_dma.c
 */

#include <haven/smmu.h>
#include <stdio.h>
#include <assert.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg) do { printf("[FAIL] %s\n", msg); assert(0); } while(0)

/**
 * Test: Initialize SMMU subsystem.
 */
static void test_smmu_init(void) {
	hv_status_t status = hv_smmu_init();
	assert(status == HV_OK);
	TEST_PASS("smmu_init: basic initialization");
}

/**
 * Test: Allocate StreamID for device.
 */
static void test_smmu_allocate_streamid(void) {
	hv_smmu_init();

	hv_u16 sid1, sid2;
	hv_status_t status;

	/* Allocate first device. */
	status = hv_smmu_allocate_streamid(0, &sid1);
	assert(status == HV_OK);
	assert(sid1 == 0);
	TEST_PASS("smmu_allocate_streamid: first device allocated");

	/* Allocate second device to different partition. */
	status = hv_smmu_allocate_streamid(1, &sid2);
	assert(status == HV_OK);
	assert(sid2 == 1);
	TEST_PASS("smmu_allocate_streamid: second device allocated");

	/* Negative: NULL output pointer. */
	status = hv_smmu_allocate_streamid(2, NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_allocate_streamid: rejects NULL streamid pointer");

	/* Negative: Invalid partition. */
	status = hv_smmu_allocate_streamid(HV_MAX_SMMU_PARTITIONS + 1, &sid1);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_allocate_streamid: rejects invalid partition");
}

/**
 * Test: Configure DMA window.
 */
static void test_smmu_configure_dma_window(void) {
	hv_smmu_init();

	hv_u16 sid;
	hv_status_t status;

	/* Allocate device. */
	status = hv_smmu_allocate_streamid(0, &sid);
	assert(status == HV_OK);

	/* Configure basic DMA window. */
	status = hv_smmu_configure_dma_window(
		sid,
		0x80000000,    /* DMA base (IOVA) */
		0x10000000,    /* 256 MB window, power-of-2 */
		0xc0000000,    /* Physical base */
		HV_DMA_RW
	);
	assert(status == HV_OK);
	TEST_PASS("smmu_configure_dma_window: basic configuration");

	/* Negative: Invalid StreamID. */
	status = hv_smmu_configure_dma_window(
		HV_MAX_SMMU_DEVICES + 1,
		0x80000000,
		0x10000000,
		0xc0000000,
		HV_DMA_RW
	);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_configure_dma_window: rejects invalid streamid");

	/* Negative: Unallocated StreamID. */
	status = hv_smmu_configure_dma_window(
		10,
		0x80000000,
		0x10000000,
		0xc0000000,
		HV_DMA_RW
	);
	assert(status == HV_EPERM);
	TEST_PASS("smmu_configure_dma_window: rejects unallocated streamid");

	/* Negative: Misaligned DMA address. */
	status = hv_smmu_configure_dma_window(
		sid,
		0x80000001,    /* Not 4K aligned */
		0x10000000,
		0xc0000000,
		HV_DMA_RW
	);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_configure_dma_window: rejects misaligned DMA address");

	/* Negative: Misaligned physical address. */
	status = hv_smmu_configure_dma_window(
		sid,
		0x80000000,
		0x10000000,
		0xc0000001,    /* Not 4K aligned */
		HV_DMA_RW
	);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_configure_dma_window: rejects misaligned physical address");

	/* Negative: Size not power-of-2. */
	status = hv_smmu_configure_dma_window(
		sid,
		0x80000000,
		0x10000001,    /* Not power-of-2 */
		0xc0000000,
		HV_DMA_RW
	);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_configure_dma_window: rejects non-power-of-2 size");
}

/**
 * Test: DMA window overlap detection.
 */
static void test_smmu_window_overlap_detection(void) {
	hv_smmu_init();

	hv_u16 sid1, sid2;
	hv_status_t status;

	/* Allocate two devices. */
	status = hv_smmu_allocate_streamid(0, &sid1);
	assert(status == HV_OK);
	status = hv_smmu_allocate_streamid(0, &sid2);
	assert(status == HV_OK);

	/* Configure first device's DMA window. */
	status = hv_smmu_configure_dma_window(
		sid1,
		0x80000000,
		0x10000000,    /* 0x80000000 - 0x90000000 */
		0xc0000000,
		HV_DMA_RW
	);
	assert(status == HV_OK);
	TEST_PASS("smmu_window_overlap: first window configured");

	/* Negative: Second device overlapping window. */
	status = hv_smmu_configure_dma_window(
		sid2,
		0x88000000,    /* Overlaps with first window */
		0x08000000,
		0xd0000000,
		HV_DMA_RW
	);
	assert(status == HV_ENOSPC);
	TEST_PASS("smmu_window_overlap: detects overlapping window");

	/* Positive: Non-overlapping window is allowed. */
	status = hv_smmu_configure_dma_window(
		sid2,
		0x90000000,    /* After first window */
		0x10000000,
		0xd0000000,
		HV_DMA_RW
	);
	assert(status == HV_OK);
	TEST_PASS("smmu_window_overlap: allows non-overlapping window");
}

/**
 * Test: Revoke DMA access.
 */
static void test_smmu_revoke_dma_access(void) {
	hv_smmu_init();

	hv_u16 sid;
	hv_status_t status;

	/* Allocate and configure device. */
	status = hv_smmu_allocate_streamid(0, &sid);
	assert(status == HV_OK);
	status = hv_smmu_configure_dma_window(sid, 0x80000000, 0x10000000, 0xc0000000, HV_DMA_RW);
	assert(status == HV_OK);

	/* Revoke DMA access. */
	status = hv_smmu_revoke_dma_access(sid);
	assert(status == HV_OK);
	TEST_PASS("smmu_revoke_dma_access: successful revocation");

	/* Negative: Double revoke. */
	status = hv_smmu_revoke_dma_access(sid);
	assert(status == HV_EPERM);
	TEST_PASS("smmu_revoke_dma_access: prevents double revocation");

	/* Negative: Invalid StreamID. */
	status = hv_smmu_revoke_dma_access(HV_MAX_SMMU_DEVICES + 1);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_revoke_dma_access: rejects invalid streamid");
}

/**
 * Test: Check DMA access permission.
 */
static void test_smmu_check_dma_access(void) {
	hv_smmu_init();

	hv_u16 sid_rw, sid_ro;
	hv_status_t status;

	/* Setup read-write device. */
	status = hv_smmu_allocate_streamid(0, &sid_rw);
	assert(status == HV_OK);
	status = hv_smmu_configure_dma_window(
		sid_rw,
		0x80000000,
		0x10000000,
		0xc0000000,
		HV_DMA_RW
	);
	assert(status == HV_OK);

	/* Setup read-only device. */
	status = hv_smmu_allocate_streamid(0, &sid_ro);
	assert(status == HV_OK);
	status = hv_smmu_configure_dma_window(
		sid_ro,
		0x90000000,
		0x10000000,
		0xd0000000,
		HV_DMA_RO
	);
	assert(status == HV_OK);

	/* RW device can read. */
	status = hv_smmu_check_dma_access(sid_rw, 0x80000000, 0x1000, HV_DMA_RO);
	assert(status == HV_OK);
	TEST_PASS("smmu_check_dma_access: RW device permits read");

	/* RW device can write. */
	status = hv_smmu_check_dma_access(sid_rw, 0x80000000, 0x1000, HV_DMA_WO);
	assert(status == HV_OK);
	TEST_PASS("smmu_check_dma_access: RW device permits write");

	/* RO device can read. */
	status = hv_smmu_check_dma_access(sid_ro, 0x90000000, 0x1000, HV_DMA_RO);
	assert(status == HV_OK);
	TEST_PASS("smmu_check_dma_access: RO device permits read");

	/* Negative: RO device cannot write. */
	status = hv_smmu_check_dma_access(sid_ro, 0x90000000, 0x1000, HV_DMA_WO);
	assert(status == HV_EPERM);
	TEST_PASS("smmu_check_dma_access: RO device denies write");

	/* Negative: Access outside DMA window. */
	status = hv_smmu_check_dma_access(sid_rw, 0xa0000000, 0x1000, HV_DMA_RW);
	assert(status == HV_EPERM);
	TEST_PASS("smmu_check_dma_access: denies access outside window");

	/* Negative: Unallocated StreamID. */
	status = hv_smmu_check_dma_access(50, 0x80000000, 0x1000, HV_DMA_RW);
	assert(status == HV_EPERM);
	TEST_PASS("smmu_check_dma_access: denies unallocated streamid");
}

/**
 * Test: Reset partition (revoke all devices).
 */
static void test_smmu_reset_partition(void) {
	hv_smmu_init();

	hv_u16 sid1, sid2;
	hv_status_t status;

	/* Allocate two devices to partition 0. */
	status = hv_smmu_allocate_streamid(0, &sid1);
	assert(status == HV_OK);
	status = hv_smmu_allocate_streamid(0, &sid2);
	assert(status == HV_OK);

	/* Configure both. */
	status = hv_smmu_configure_dma_window(sid1, 0x80000000, 0x10000000, 0xc0000000, HV_DMA_RW);
	assert(status == HV_OK);
	status = hv_smmu_configure_dma_window(sid2, 0x90000000, 0x10000000, 0xd0000000, HV_DMA_RW);
	assert(status == HV_OK);

	/* Reset partition 0. */
	status = hv_smmu_reset_partition(0);
	assert(status == HV_OK);
	TEST_PASS("smmu_reset_partition: partition reset successful");

	/* Verify both devices are revoked. */
	hv_device_dma_t dma_info;
	status = hv_smmu_get_device_dma(sid1, &dma_info);
	assert(status == HV_EPERM);  /* Device no longer configured */
	TEST_PASS("smmu_reset_partition: sid1 revoked");

	status = hv_smmu_get_device_dma(sid2, &dma_info);
	assert(status == HV_EPERM);  /* Device no longer configured */
	TEST_PASS("smmu_reset_partition: sid2 revoked");

	/* Negative: Invalid partition. */
	status = hv_smmu_reset_partition(HV_MAX_SMMU_PARTITIONS + 1);
	assert(status == HV_EINVAL);
	TEST_PASS("smmu_reset_partition: rejects invalid partition");
}

/**
 * Main test runner.
 */
int main(void) {
	printf("\n=== SMMU DMA Isolation Unit Tests ===\n\n");

	test_smmu_init();
	test_smmu_allocate_streamid();
	test_smmu_configure_dma_window();
	test_smmu_window_overlap_detection();
	test_smmu_revoke_dma_access();
	test_smmu_check_dma_access();
	test_smmu_reset_partition();

	printf("\n=== All SMMU tests passed ===\n\n");
	return 0;
}
