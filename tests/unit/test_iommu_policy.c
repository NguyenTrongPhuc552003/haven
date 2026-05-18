/**
 * Unit tests for IOMMU group ownership policy.
 *
 * Tests cover:
 *   - Initialization
 *   - Group assignment and idempotent re-assignment
 *   - Reassignment denial across partitions (HV_EPERM)
 *   - Release by owner and double-release denial
 *   - Non-owner release denial
 *   - check_group positive and negative paths
 *   - Boundary validation (group 0, partition 0, out-of-range)
 *   - Full lifecycle: assign → release → reassign to new partition
 *
 * @file tests/unit/test_iommu_policy.c
 */

#include <assert.h>
#include <haven/iommu.h>
#include <stdio.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg)                                                         \
  do {                                                                         \
    printf("[FAIL] %s\n", msg);                                                \
    assert(0);                                                                 \
  } while (0)

static void test_iommu_init(void) {
  hv_status_t s = hv_iommu_init();
  assert(s == HV_OK);
  TEST_PASS("iommu_init: basic initialization");
}

static void test_iommu_assign_group(void) {
  hv_iommu_init();

  hv_status_t s;

  /* Valid assignment. */
  s = hv_iommu_assign_group(1, 1);
  assert(s == HV_OK);
  TEST_PASS("iommu_assign_group: valid assignment");

  /* Idempotent: same partition re-assigns same group. */
  s = hv_iommu_assign_group(1, 1);
  assert(s == HV_OK);
  TEST_PASS("iommu_assign_group: idempotent re-assignment by same owner");

  /* Conflict: different partition tries to claim assigned group. */
  s = hv_iommu_assign_group(1, 2);
  assert(s == HV_EPERM);
  TEST_PASS("iommu_assign_group: denies reassignment to different partition");

  /* Different group can be assigned to any partition. */
  s = hv_iommu_assign_group(2, 2);
  assert(s == HV_OK);
  TEST_PASS("iommu_assign_group: different group to different partition");

  /* Negative: group 0 invalid. */
  s = hv_iommu_assign_group(0, 1);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_assign_group: rejects group 0");

  /* Negative: partition 0 invalid. */
  s = hv_iommu_assign_group(3, 0);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_assign_group: rejects partition 0");

  /* Negative: group out-of-range. */
  s = hv_iommu_assign_group(256, 1);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_assign_group: rejects group >= HV_MAX_IOMMU_GROUPS");

  /* Negative: partition out-of-range. */
  s = hv_iommu_assign_group(4, 256);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_assign_group: rejects partition >= HV_MAX_IOMMU_PARTITIONS");
}

static void test_iommu_release_group(void) {
  hv_iommu_init();

  hv_status_t s;

  hv_iommu_assign_group(1, 1);

  /* Owner releases. */
  s = hv_iommu_release_group(1, 1);
  assert(s == HV_OK);
  TEST_PASS("iommu_release_group: owner can release");

  /* Double release denied. */
  s = hv_iommu_release_group(1, 1);
  assert(s == HV_EPERM);
  TEST_PASS("iommu_release_group: denies double release");

  /* Non-owner release denied. */
  hv_iommu_assign_group(2, 2);
  s = hv_iommu_release_group(2, 3);
  assert(s == HV_EPERM);
  TEST_PASS("iommu_release_group: denies release by non-owner");

  /* Negative: group 0. */
  s = hv_iommu_release_group(0, 1);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_release_group: rejects group 0");

  /* Negative: partition 0. */
  s = hv_iommu_release_group(2, 0);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_release_group: rejects partition 0");
}

static void test_iommu_check_group(void) {
  hv_iommu_init();

  hv_status_t s;

  hv_iommu_assign_group(5, 7);

  /* Correct owner. */
  s = hv_iommu_check_group(5, 7);
  assert(s == HV_OK);
  TEST_PASS("iommu_check_group: correct owner accepted");

  /* Wrong partition. */
  s = hv_iommu_check_group(5, 8);
  assert(s == HV_EPERM);
  TEST_PASS("iommu_check_group: rejects wrong partition");

  /* Unassigned group. */
  s = hv_iommu_check_group(10, 7);
  assert(s == HV_EPERM);
  TEST_PASS("iommu_check_group: rejects unassigned group");

  /* Negative: group 0. */
  s = hv_iommu_check_group(0, 7);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_check_group: rejects group 0");

  /* Negative: partition 0. */
  s = hv_iommu_check_group(5, 0);
  assert(s == HV_EINVAL);
  TEST_PASS("iommu_check_group: rejects partition 0");
}

static void test_iommu_lifecycle(void) {
  hv_iommu_init();

  hv_status_t s;

  /* Partition 1 owns group 3. */
  s = hv_iommu_assign_group(3, 1);
  assert(s == HV_OK);
  s = hv_iommu_check_group(3, 1);
  assert(s == HV_OK);

  /* Partition 2 cannot take it. */
  s = hv_iommu_assign_group(3, 2);
  assert(s == HV_EPERM);

  /* Partition 1 releases. */
  s = hv_iommu_release_group(3, 1);
  assert(s == HV_OK);

  /* Now partition 2 can claim it. */
  s = hv_iommu_assign_group(3, 2);
  assert(s == HV_OK);
  s = hv_iommu_check_group(3, 2);
  assert(s == HV_OK);

  /* Partition 1 no longer owns it. */
  s = hv_iommu_check_group(3, 1);
  assert(s == HV_EPERM);

  TEST_PASS("iommu_lifecycle: assign → deny-foreign → release → reassign");
}

int main(void) {
  printf("\n=== IOMMU Group Ownership Policy Unit Tests ===\n\n");

  test_iommu_init();
  test_iommu_assign_group();
  test_iommu_release_group();
  test_iommu_check_group();
  test_iommu_lifecycle();

  printf("\n=== All IOMMU policy tests passed ===\n\n");
  return 0;
}
