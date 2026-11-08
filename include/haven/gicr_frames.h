/* SPDX-License-Identifier: Apache-2.0 */
/**
 * GICv3/v4 redistributor frames found by affinity (D2, ledger row 25).
 *
 * A redistributor region is a run of frames, each starting with RD_base;
 * GICR_TYPER (RD_base + 0x08, 64-bit) gives the frame's CPU affinity in
 * bits [63:32] (Aff3.Aff2.Aff1.Aff0), VLPIS in bit 1 (the frame is four
 * 64 KiB pages, 256 KiB, instead of two) and Last in bit 4 (the last frame
 * of the region). Arm IHI 0069, GICR_TYPER. Pure: reads memory only.
 *
 * @file include/haven/gicr_frames.h
 */

#ifndef HAVEN_GICR_FRAMES_H
#define HAVEN_GICR_FRAMES_H

#include <haven/types.h>
#include <stdint.h>

#define HV_GICR_MAX_FRAMES 64U /* walk bound if no frame says Last */

/** Walk the region at `base` and set frame[i] to the RD_base whose
 * affinity equals mpidr[i]'s (MPIDR Aff3 [39:32], Aff2..Aff0 [23:0]), or
 * to 0 if none does. Returns how many of the `ncpus` CPUs were found. */
hv_u32 hv_gicr_find_frames(uintptr_t base, const hv_u64 *mpidr, hv_u32 ncpus,
			   uintptr_t *frame);

#endif /* HAVEN_GICR_FRAMES_H */
