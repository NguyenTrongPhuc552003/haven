/* SPDX-License-Identifier: Apache-2.0 */
/* MM1/MM2: portable stage-2 permission bits.
 *
 * Checks (host, L1 evidence only - no hardware binding is exercised):
 *   1. hv_stage2_attrs_valid() accepts exactly the six approved combinations
 *      and rejects 0, unknown bits, write/execute without read, and
 *      device+execute.
 *   2. hv_stage2_map_partition() rejects an invalid attrs with HV_EINVAL and
 *      leaves no partition state behind.
 *   3. hv_s2_hw_attrs() produces the descriptor bits in the MM1 proposal,
 *      including XN[1:0] = 0b10 (never 0b11) for non-executable mappings. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arch/arm64/include/asm/s2_attrs.h"
#include "haven/stage2.h"

#define XN_FIELD(hw) (((hv_u64)(hw) >> 53) & 3U)

static void test_validator(void)
{
	const hv_u64 valid[] = {
		HV_MEM_R,   HV_MEM_R | HV_MEM_W,      HV_MEM_R | HV_MEM_X,
		HV_MEM_RWX, HV_MEM_DEVICE | HV_MEM_R, HV_MEM_DEVICE_RW,
	};
	const hv_u64 invalid[] = {
		0U,
		HV_MEM_W,
		HV_MEM_X,
		HV_MEM_W | HV_MEM_X,
		HV_MEM_DEVICE,
		HV_MEM_DEVICE | HV_MEM_R | HV_MEM_X,
		HV_MEM_DEVICE | HV_MEM_RWX,
		HV_MEM_R | (1ULL << 4),
		HV_MEM_RWX | (1ULL << 63),
	};
	unsigned i;

	for (i = 0U; i < sizeof(valid) / sizeof(valid[0]); ++i) {
		assert(hv_stage2_attrs_valid(valid[i]) == 1U);
	}
	for (i = 0U; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
		assert(hv_stage2_attrs_valid(invalid[i]) == 0U);
	}
}

static void test_map_rejects_without_state_change(void)
{
	struct hv_mem_region bad[1] = {{.ipa_base = 0x1000,
					.pa_base = 0x2000,
					.size = 0x1000,
					.attrs = 0U}};
	struct hv_mem_region good[1] = {{.ipa_base = 0x1000,
					 .pa_base = 0x2000,
					 .size = 0x1000,
					 .attrs = HV_MEM_R}};
	struct hv_partition_mem cfg_bad = {
		.partition_id = 7U, .regions = bad, .region_count = 1U};
	struct hv_partition_mem cfg_good = {
		.partition_id = 7U, .regions = good, .region_count = 1U};

	assert(hv_stage2_init() == HV_OK);
	assert(hv_stage2_map_partition(&cfg_bad) == HV_EINVAL);
	/* No state left behind: the partition is not mapped... */
	assert(hv_stage2_unmap_partition(7U) == HV_EPERM);
	/* ...and the same partition can still be mapped validly. */
	assert(hv_stage2_map_partition(&cfg_good) == HV_OK);
	assert(hv_stage2_unmap_partition(7U) == HV_OK);
}

static void test_translation(void)
{
	const uint64_t normal = (uint64_t)(S2_MEMATTR_NORMAL_WB | S2_SH_IS);
	const uint64_t device = (uint64_t)(S2_MEMATTR_DEVICE_nGnRE | S2_SH_NS);

	assert(hv_s2_hw_attrs(HV_MEM_R) == (normal | S2AP_RO | S2_XN_NEVER));
	assert(hv_s2_hw_attrs(HV_MEM_R | HV_MEM_W) ==
	       (normal | S2AP_RW | S2_XN_NEVER));
	assert(hv_s2_hw_attrs(HV_MEM_R | HV_MEM_X) == (normal | S2AP_RO));
	assert(hv_s2_hw_attrs(HV_MEM_RWX) == (normal | S2AP_RW));
	assert(hv_s2_hw_attrs(HV_MEM_DEVICE | HV_MEM_R) ==
	       (device | S2AP_RO | S2_XN_NEVER));
	assert(hv_s2_hw_attrs(HV_MEM_DEVICE_RW) ==
	       (device | S2AP_RW | S2_XN_NEVER));

	/* MM2: execute-never is XN[1:0] = 0b10, which is non-executable with or
	 * without FEAT_XNX; 0b11 would be executable at EL1 under FEAT_XNX. */
	assert(XN_FIELD(hv_s2_hw_attrs(HV_MEM_R)) == 2U);
	assert(XN_FIELD(hv_s2_hw_attrs(HV_MEM_DEVICE_RW)) == 2U);
	assert(XN_FIELD(hv_s2_hw_attrs(HV_MEM_RWX)) == 0U);
	assert(XN_FIELD(S2_XN_ALL) == 2U);
}

int main(void)
{
	test_validator();
	test_map_rejects_without_state_change();
	test_translation();
	printf("test_stage2_attrs: all checks passed\n");
	return 0;
}
