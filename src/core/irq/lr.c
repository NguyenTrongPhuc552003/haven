/* SPDX-License-Identifier: Apache-2.0 */
/**
 * List-register decisions, pure (include/haven/lr.h).
 *
 * @file src/core/irq/lr.c
 */

#include <haven/lr.h>

enum hv_lr_action hv_lr_choose(const hv_u64 *lrs, unsigned int n, hv_u32 vintid,
			       unsigned int *slot)
{
	unsigned int free_slot = n;

	for (unsigned int i = 0; i < n; i++) {
		hv_u64 st = lrs[i] & HV_LR_STATE_FIELD;

		if (st == 0ULL) {
			if (free_slot == n)
				free_slot = i;
			continue;
		}
		if ((lrs[i] & HV_LR_VINTID_FIELD) != vintid)
			continue;
		if ((st & HV_LR_ST_PENDING) != 0ULL)
			return HV_LR_NOTHING;
		/* Active only. Arm allows pending and active for software
		 * interrupts; a hardware one waits for its own deactivation. */
		if ((lrs[i] & HV_LR_HW_BIT) != 0ULL)
			return HV_LR_FULL;
		*slot = i;
		return HV_LR_MAKE_PA;
	}
	if (free_slot == n)
		return HV_LR_FULL;
	*slot = free_slot;
	return HV_LR_WRITE;
}

int hv_lr_wait_add(struct hv_lr_wait *w, hv_u32 intid)
{
	if (intid < 16U)
		return -1;
	if (intid < 32U) {
		w->ppis |= 1U << (intid - 16U);
		return 0;
	}
	for (hv_u32 i = 0; i < w->nspi; i++)
		if (w->spi[i] == intid)
			return 0;
	if (w->nspi >= HV_LR_WAIT_SPIS)
		return -1;
	w->spi[w->nspi++] = intid;
	return 0;
}

int hv_lr_wait_take(struct hv_lr_wait *w, hv_u32 *intid)
{
	if (w->ppis != 0U) {
		hv_u32 b = 0U;

		while ((w->ppis & (1U << b)) == 0U)
			b++;
		w->ppis &= ~(1U << b);
		*intid = 16U + b;
		return 1;
	}
	if (w->nspi == 0U)
		return 0;
	*intid = w->spi[0];
	for (hv_u32 i = 1; i < w->nspi; i++)
		w->spi[i - 1U] = w->spi[i];
	w->nspi--;
	return 1;
}

int hv_lr_wait_any(const struct hv_lr_wait *w)
{
	return w->ppis != 0U || w->nspi != 0U;
}
