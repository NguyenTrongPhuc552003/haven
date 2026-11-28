/* SPDX-License-Identifier: Apache-2.0 */
/**
 * List-register decisions, pure (docs/methodology/stage2/
 * 20261124-lr-overflow/).
 *
 * hv_lr_choose decides what one injection does given the current list
 * registers: Arm makes two list registers with the same vINTID in a
 * non-Invalid state UNPREDICTABLE (ICH_LR<n>_EL2, vINTID), so a vINTID
 * already there is not written again. struct hv_lr_wait holds the
 * hardware interrupts that found no free list register, until the
 * maintenance interrupt says one is free.
 *
 * @file include/haven/lr.h
 */

#ifndef HAVEN_LR_H
#define HAVEN_LR_H

#include <haven/types.h>

/* ICH_LR<n>_EL2 fields used here */
#define HV_LR_STATE_SHIFT 62U
#define HV_LR_STATE_FIELD (3ULL << HV_LR_STATE_SHIFT)
#define HV_LR_ST_PENDING (1ULL << HV_LR_STATE_SHIFT)
#define HV_LR_ST_ACTIVE (2ULL << HV_LR_STATE_SHIFT)
#define HV_LR_HW_BIT (1ULL << 61U)
#define HV_LR_VINTID_FIELD 0xFFFFFFFFULL

enum hv_lr_action {
	HV_LR_WRITE, /* write a new list register at *slot */
	HV_LR_NOTHING, /* the vINTID is already pending there */
	HV_LR_MAKE_PA, /* *slot holds it Active: make it pending and active */
	HV_LR_FULL, /* no free list register (or a hardware copy active) */
};

/* What injecting `vintid` does, given the n list-register values; *slot is
 * the LR to write for HV_LR_WRITE and HV_LR_MAKE_PA. */
enum hv_lr_action hv_lr_choose(hv_u32 vintid, const hv_u64 *lrs, unsigned int n,
			       unsigned int *slot);

/* Hardware interrupts waiting for a list register, per CPU: the PPIs as a
 * bit mask, up to HV_LR_WAIT_SPIS SPIs in arrival order. */
#define HV_LR_WAIT_SPIS 8U
struct hv_lr_wait {
	hv_u32 ppis; /* bit = INTID - 16 */
	hv_u32 nspi;
	hv_u32 spi[HV_LR_WAIT_SPIS];
};

/* 0 if `intid` waits now (or already did), -1 if it cannot (an SGI, or
 * the SPI slots are full). */
int hv_lr_wait_add(struct hv_lr_wait *w, hv_u32 intid);
/* 1 and the next waiting INTID (PPIs first, lowest first, then SPIs in
 * arrival order), or 0 if nothing waits. */
int hv_lr_wait_take(struct hv_lr_wait *w, hv_u32 *intid);
/* 1 if anything waits. */
int hv_lr_wait_any(const struct hv_lr_wait *w);

#endif /* HAVEN_LR_H */
