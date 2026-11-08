/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Redistributor frames found by GICR_TYPER affinity (D2). See
 * include/haven/gicr_frames.h.
 *
 * @file src/core/irq/gicr_frames.c
 */

#include <haven/gicr_frames.h>

#define TYPER_LO 0x08U
#define TYPER_HI 0x0CU
#define TYPER_VLPIS (1U << 1)
#define TYPER_LAST (1U << 4)
#define FRAME_2PAGES 0x20000U /* RD + SGI */
#define FRAME_4PAGES 0x40000U /* RD + SGI + VLPI + reserved (GICv4) */

static hv_u32 rd32(uintptr_t a)
{
	return *(volatile const hv_u32 *)a;
}

hv_u32 hv_gicr_find_frames(uintptr_t base, const hv_u64 *mpidr, hv_u32 ncpus,
			   uintptr_t *frame)
{
	hv_u32 found = 0U;

	for (hv_u32 i = 0; i < ncpus; i++)
		frame[i] = 0U;
	for (hv_u32 n = 0; n < HV_GICR_MAX_FRAMES; n++) {
		hv_u32 lo = rd32(base + TYPER_LO);
		hv_u32 aff = rd32(base + TYPER_HI);

		for (hv_u32 i = 0; i < ncpus; i++) {
			hv_u32 want =
				(hv_u32)(((mpidr[i] >> 32) & 0xFFU) << 24) |
				(hv_u32)(mpidr[i] & 0xFFFFFFU);

			if (frame[i] == 0U && aff == want) {
				frame[i] = base;
				found++;
			}
		}
		if ((lo & TYPER_LAST) != 0U)
			break;
		base += (lo & TYPER_VLPIS) != 0U ? FRAME_4PAGES : FRAME_2PAGES;
	}
	return found;
}
