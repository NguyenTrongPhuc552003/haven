/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Host tests for the list-register decisions (rows U1 and U2 of
 * docs/methodology/stage2/20261124-lr-overflow/README.md).
 *
 * @file tests/unit/test_lr.c
 */

#include <assert.h>
#include <haven/lr.h>
#include <stdio.h>

#define P HV_LR_ST_PENDING
#define A HV_LR_ST_ACTIVE
#define PA (HV_LR_ST_PENDING | HV_LR_ST_ACTIVE)
#define HW HV_LR_HW_BIT

static enum hv_lr_action choose(const hv_u64 *lrs, unsigned int n, hv_u32 id,
				unsigned int *slot)
{
	*slot = 99U;
	return hv_lr_choose(lrs, n, id, slot);
}

/* U1: each row of the design's table */
static void test_choose(void)
{
	unsigned int s;
	hv_u64 empty[4] = {0, 0, 0, 0};
	hv_u64 one_p[4] = {P | 1U, 0, 0, 0};
	hv_u64 one_pa[4] = {0, PA | 1U, 0, 0};
	hv_u64 one_a[4] = {P | 27U, 0, A | 1U, 0};
	hv_u64 hw_a[4] = {A | HW | 27U, 0, 0, 0};
	hv_u64 full[2] = {P | 3U, A | 27U | HW};
	hv_u64 full_a[2] = {P | 3U, A | 1U};
	hv_u64 other[4] = {P | 2U, 0, A | 5U, 0};

	assert(choose(empty, 4, 1U, &s) == HV_LR_WRITE && s == 0U);
	/* already Pending, or Pending and active: nothing */
	assert(choose(one_p, 4, 1U, &s) == HV_LR_NOTHING && s == 99U);
	assert(choose(one_pa, 4, 1U, &s) == HV_LR_NOTHING);
	/* Active software copy: that slot, made pending and active */
	assert(choose(one_a, 4, 1U, &s) == HV_LR_MAKE_PA && s == 2U);
	/* Active hardware copy: waits */
	assert(choose(hw_a, 4, 27U, &s) == HV_LR_FULL);
	/* the first free slot, past other vINTIDs */
	assert(choose(other, 4, 1U, &s) == HV_LR_WRITE && s == 1U);
	/* full */
	assert(choose(full, 2, 1U, &s) == HV_LR_FULL);
	/* full, but the same software vINTID Active: made pending and active */
	assert(choose(full_a, 2, 1U, &s) == HV_LR_MAKE_PA && s == 1U);
	/* the state decides, not leftover bits: an Invalid LR with the same
	 * vINTID is a free slot */
	{
		hv_u64 stale[2] = {1U, 0};

		assert(choose(stale, 2, 1U, &s) == HV_LR_WRITE && s == 0U);
	}
	printf("[PASS] U1: one decision per injection\n");
}

/* U2: the waiting set */
static void test_wait(void)
{
	struct hv_lr_wait w = {0};
	hv_u32 id;

	assert(!hv_lr_wait_any(&w) && hv_lr_wait_take(&w, &id) == 0);
	assert(hv_lr_wait_add(&w, 5U) == -1); /* an SGI never waits here */
	assert(hv_lr_wait_add(&w, 40U) == 0);
	assert(hv_lr_wait_add(&w, 27U) == 0);
	assert(hv_lr_wait_add(&w, 26U) == 0);
	assert(hv_lr_wait_add(&w, 27U) == 0); /* twice: once */
	assert(hv_lr_wait_add(&w, 33U) == 0);
	assert(hv_lr_wait_add(&w, 40U) == 0); /* twice: once */
	assert(hv_lr_wait_any(&w));
	/* PPIs first, lowest first, then SPIs in arrival order */
	assert(hv_lr_wait_take(&w, &id) == 1 && id == 26U);
	assert(hv_lr_wait_take(&w, &id) == 1 && id == 27U);
	assert(hv_lr_wait_take(&w, &id) == 1 && id == 40U);
	assert(hv_lr_wait_take(&w, &id) == 1 && id == 33U);
	assert(hv_lr_wait_take(&w, &id) == 0 && !hv_lr_wait_any(&w));
	/* eight SPIs fit, the ninth is refused */
	for (hv_u32 i = 0; i < HV_LR_WAIT_SPIS; i++)
		assert(hv_lr_wait_add(&w, 32U + i) == 0);
	assert(hv_lr_wait_add(&w, 100U) == -1);
	assert(hv_lr_wait_add(&w, 31U) == 0); /* PPIs still fit */
	printf("[PASS] U2: waiting set order and limit\n");
}

int main(void)
{
	test_choose();
	test_wait();
	printf("=== all list-register tests PASS ===\n");
	return 0;
}
