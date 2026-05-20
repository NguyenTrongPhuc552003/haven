#ifndef HAVEN_CPU_CONTEXT_H
#define HAVEN_CPU_CONTEXT_H

#include <haven/types.h>

struct hv_cpu_context {
	hv_u64 x[30];
	hv_u64 x30;
	hv_u64 sp_el0;
	hv_u64 sp_el1;
	hv_u64 elr_el1;
	hv_u64 spsr_el1;
	hv_u64 sctlr_el1;
	hv_u64 ttbr0_el1;
	hv_u64 ttbr1_el1;
	hv_u64 tcr_el1;
	hv_u64 mair_el1;
	hv_u64 amair_el1;
	hv_u64 vbar_el1;
	hv_u64 esr_el1;
	hv_u64 far_el1;
	hv_u64 afsr0_el1;
	hv_u64 afsr1_el1;
	hv_u64 tpidr_el0;
	hv_u64 tpidr_el1;
	hv_u64 tpidrro_el0;
	hv_u32 vmid;
	hv_u32 reserved;
	/* NEON/FP state: FPCR, FPSR, Q0-Q31 (16 bytes each).
	 * Must be 16-byte aligned; positioned at offset 400 so the struct
	 * itself only needs 8-byte alignment (same as before). */
	hv_u64 fpcr;
	hv_u64 fpsr;
	/* Q0-Q31 stored as pairs of uint64 (lo, hi) = 32 × 16 = 512 bytes */
	hv_u64 fp_q[64];
};

/* Compile-time offset assertions (verified in unit tests). */
#define HV_CTX_OFFSET_FPCR 400
#define HV_CTX_OFFSET_FPSR 408
#define HV_CTX_OFFSET_FP_Q 416

#endif
