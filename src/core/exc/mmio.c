/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Emulated MMIO: syndrome decoding, frame access and region dispatch.
 *
 * @file src/core/exc/mmio.c
 */

#include <haven/mmio.h>

#define ESR_ISV (1ULL << 24)
#define ESR_SAS_SHIFT 22U
#define ESR_SSE (1ULL << 21)
#define ESR_SRT_SHIFT 16U
#define ESR_SF (1ULL << 15)
#define ESR_WNR (1ULL << 6)

int hv_mmio_decode(hv_u64 esr, hv_mmio_access_t *a)
{
	if ((esr & ESR_ISV) == 0U)
		return -1;
	a->size = 1U << ((esr >> ESR_SAS_SHIFT) & 3U);
	a->reg = (hv_u32)((esr >> ESR_SRT_SHIFT) & 0x1FU);
	a->write = (esr & ESR_WNR) != 0U;
	a->sign_extend = (esr & ESR_SSE) != 0U;
	a->reg64 = (esr & ESR_SF) != 0U;
	return 0;
}

/* x0..x18 -> slots 0..18, x29 -> 19, x30 -> 20, x19..x28 -> 24..33 */
static int frame_slot(hv_u32 n)
{
	if (n <= 18U)
		return (int)n;
	if (n <= 28U)
		return (int)(24U + (n - 19U));
	if (n == 29U)
		return 19;
	if (n == 30U)
		return 20;
	return -1; /* 31: XZR */
}

hv_u64 hv_frame_get(const hv_u64 *frame, hv_u32 n)
{
	int s = frame_slot(n);

	return s < 0 ? 0U : frame[s];
}

void hv_frame_set(hv_u64 *frame, hv_u32 n, hv_u64 v)
{
	int s = frame_slot(n);

	if (s >= 0)
		frame[s] = v;
}

static hv_u64 size_mask(hv_u32 size)
{
	return size >= 8U ? ~0ULL : ((1ULL << (size * 8U)) - 1U);
}

hv_u64 hv_mmio_load_value(const hv_mmio_access_t *a, hv_u64 raw)
{
	hv_u64 v = raw & size_mask(a->size);

	if (a->sign_extend && a->size < 8U) {
		hv_u64 sign = 1ULL << (a->size * 8U - 1U);
		v = (v ^ sign) - sign; /* sign-extend to 64 bits */
	}
	if (!a->reg64)
		v &= 0xFFFFFFFFULL; /* writing Wt clears the upper half */
	return v;
}

hv_u64 hv_mmio_store_value(const hv_mmio_access_t *a, hv_u64 reg)
{
	return reg & size_mask(a->size);
}

static struct {
	hv_u32 cpu;
	hv_u64 ipa;
	hv_u64 size;
	hv_mmio_handler_t fn;
	void *ctx;
} regions[HV_MMIO_MAX_REGIONS];
static hv_u32 nregions;

hv_status_t hv_mmio_register(hv_u32 cpu, hv_u64 ipa, hv_u64 size,
			     hv_mmio_handler_t fn, void *ctx)
{
	if (fn == NULL || size == 0U || ipa + size < ipa)
		return HV_EINVAL;
	for (hv_u32 i = 0; i < nregions; i++) {
		if (regions[i].cpu == cpu &&
		    ipa < regions[i].ipa + regions[i].size &&
		    regions[i].ipa < ipa + size)
			return HV_EINVAL; /* overlaps an existing region */
	}
	if (nregions >= HV_MMIO_MAX_REGIONS)
		return HV_ENOSPC;
	regions[nregions].cpu = cpu;
	regions[nregions].ipa = ipa;
	regions[nregions].size = size;
	regions[nregions].fn = fn;
	regions[nregions].ctx = ctx;
	nregions++;
	return HV_OK;
}

void hv_mmio_reset(void)
{
	nregions = 0;
}

int hv_mmio_emulate(hv_u32 cpu, hv_u64 ipa, const hv_mmio_access_t *a,
		    hv_u64 *frame)
{
	for (hv_u32 i = 0; i < nregions; i++) {
		hv_u64 off;
		hv_u64 v;

		if (regions[i].cpu != cpu || ipa < regions[i].ipa ||
		    ipa - regions[i].ipa >= regions[i].size)
			continue;
		off = ipa - regions[i].ipa;
		if (off + a->size > regions[i].size)
			return -1; /* straddles the end of the region */
		if (a->write) {
			v = hv_mmio_store_value(a, hv_frame_get(frame, a->reg));
			return regions[i].fn(regions[i].ctx, off, a->size, 1,
					     &v);
		}
		v = 0;
		if (regions[i].fn(regions[i].ctx, off, a->size, 0, &v) != 0)
			return -1;
		hv_frame_set(frame, a->reg, hv_mmio_load_value(a, v));
		return 0;
	}
	return -1;
}
