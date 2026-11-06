/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Emulated MMIO for partitions: decoding a trapped load or store and
 * dispatching it to a registered device model.
 *
 * Syndrome fields: Arm ARM, ESR_EL2 ISS for a data abort (ISV bit 24,
 * SAS 23:22, SSE 21, SRT 20:16, SF 15, WnR 6).
 *
 * @file include/haven/mmio.h
 */

#ifndef HAVEN_MMIO_H
#define HAVEN_MMIO_H

#include <haven/types.h>

/* Exception frame saved by arch/arm64/entry.S (save_minimal), in 64-bit
 * slots: x0..x18 at slots 0..18, x29 at 19, x30 at 20, ELR at 21, SPSR
 * at 22, x19..x28 at slots 24..33. */
#define HV_FRAME_SIZE 272U
#define HV_FRAME_ELR 21U
#define HV_FRAME_SPSR 22U

typedef struct {
	hv_u32 size; /* access size in bytes: 1, 2, 4 or 8 */
	hv_u32 reg; /* transfer register 0..30, 31 = XZR */
	int write; /* 1 store, 0 load */
	int sign_extend; /* load: sign-extend to the register width */
	int reg64; /* load: destination is Xt (1) or Wt (0) */
} hv_mmio_access_t;

/** Decode a data-abort syndrome. Returns 0 if ISV is set (the access can
 * be emulated), -1 otherwise. Pure. */
int hv_mmio_decode(hv_u64 esr, hv_mmio_access_t *a);

/** Value of general register n in a saved frame; n = 31 reads as zero. */
hv_u64 hv_frame_get(const hv_u64 *frame, hv_u32 n);

/** Write general register n in a saved frame; n = 31 is ignored. */
void hv_frame_set(hv_u64 *frame, hv_u32 n, hv_u64 v);

/** The register value a load writes back, from the raw bytes read:
 * truncated to the access size, sign- or zero-extended, and limited to
 * 32 bits when the destination is Wt. Pure. */
hv_u64 hv_mmio_load_value(const hv_mmio_access_t *a, hv_u64 raw);

/** The bytes a store writes, from the source register value. Pure. */
hv_u64 hv_mmio_store_value(const hv_mmio_access_t *a, hv_u64 reg);

/* A device model: offset from the region base; for a load it sets *value,
 * for a store it reads *value. Returns 0 if the access was handled. */
typedef int (*hv_mmio_handler_t)(void *ctx, hv_u64 offset, hv_u32 size,
				 int write, hv_u64 *value);

#define HV_MMIO_MAX_REGIONS 8U

/** Register an emulated region for the partition running on `cpu` (one
 * vCPU per partition for now). Returns HV_OK, HV_EINVAL or HV_ENOSPC. */
hv_status_t hv_mmio_register(hv_u32 cpu, hv_u64 ipa, hv_u64 size,
			     hv_mmio_handler_t fn, void *ctx);

/** Forget all regions (tests). */
void hv_mmio_reset(void);

/**
 * Emulate one trapped access at `ipa` for `cpu`, reading or writing the
 * saved frame. Returns 0 if a registered model handled it; -1 if no
 * region matched or the model refused (the caller denies the access).
 */
int hv_mmio_emulate(hv_u32 cpu, hv_u64 ipa, const hv_mmio_access_t *a,
		    hv_u64 *frame);

#endif /* HAVEN_MMIO_H */
