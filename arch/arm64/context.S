/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/context.S - ARM64 guest CPU context save and restore.
 *
 * A full guest context includes:
 *   - General-purpose registers x0–x30
 *   - Stack pointer: SP_EL0, SP_EL1
 *   - Exception Link Register: ELR_EL1
 *   - Saved Program Status Register: SPSR_EL1
 *   - EL1 system registers: SCTLR_EL1, TTBR0_EL1, TTBR1_EL1, TCR_EL1,
 *                            MAIR_EL1, AMAIR_EL1, VBAR_EL1, ESR_EL1,
 *                            FAR_EL1, AFSR0_EL1, AFSR1_EL1,
 *                            TPIDR_EL0, TPIDR_EL1, TPIDRRO_EL0
 *   - Counter-timer: CNTVOFF_EL2 (written at restore, not read from context)
 *   - NEON/FP: Q0–Q31, FPCR, FPSR (optional - saved only if CPTR_EL2 clears TFP)
 *
 * struct hv_cpu_context layout (see include/haven/cpu_context.h):
 * Offset  Field
 *  0      x0  – x29  (30 × 8 = 240 bytes)
 *  240    x30 (LR)
 *  248    sp_el0
 *  256    sp_el1
 *  264    elr_el1
 *  272    spsr_el1
 *  280    sctlr_el1
 *  288    ttbr0_el1
 *  296    ttbr1_el1
 *  304    tcr_el1
 *  312    mair_el1
 *  320    amair_el1
 *  328    vbar_el1
 *  336    esr_el1
 *  344    far_el1
 *  352    afsr0_el1
 *  360    afsr1_el1
 *  368    tpidr_el0
 *  376    tpidr_el1
 *  384    tpidrro_el0
 *  392    vmid  (uint32_t)
 *  396    pad
 *  Total: 400 bytes
 */

/* Offsets into struct hv_cpu_context */
#define CTX_X0          0
#define CTX_X30         240
#define CTX_SP_EL0      248
#define CTX_SP_EL1      256
#define CTX_ELR_EL1     264
#define CTX_SPSR_EL1    272
#define CTX_SCTLR_EL1   280
#define CTX_TTBR0_EL1   288
#define CTX_TTBR1_EL1   296
#define CTX_TCR_EL1     304
#define CTX_MAIR_EL1    312
#define CTX_AMAIR_EL1   320
#define CTX_VBAR_EL1    328
#define CTX_ESR_EL1     336
#define CTX_FAR_EL1     344
#define CTX_AFSR0_EL1   352
#define CTX_AFSR1_EL1   360
#define CTX_TPIDR_EL0   368
#define CTX_TPIDR_EL1   376
#define CTX_TPIDRRO_EL0 384
#define CTX_VMID        392
/* NEON/FP state appended after the integer context */
#define CTX_FPCR        400
#define CTX_FPSR        408
#define CTX_Q0          416   /* Q0-Q31: 32 × 16 = 512 bytes, ends at 928 */


        .section ".text", "ax"


/* -----------------------------------------------------------------------
 * void hv_save_guest_context(struct hv_cpu_context *ctx)
 *
 * Called from the C exception handler with x0 = pointer to context struct.
 * Caller must have already saved x0 on stack (via entry.S save_minimal)
 * before calling C, so we restore x0 from the stack frame if needed.
 *
 * On entry: x0 = ctx pointer (already set by caller)
 * Clobbers: x1 (used for sysreg reads)
 * ----------------------------------------------------------------------- */

        .global hv_save_guest_context
        .align  4
hv_save_guest_context:
        /* Save general-purpose registers x0–x29 */
        stp     x0,  x1,  [x0, #CTX_X0 + 0]
        stp     x2,  x3,  [x0, #CTX_X0 + 16]
        stp     x4,  x5,  [x0, #CTX_X0 + 32]
        stp     x6,  x7,  [x0, #CTX_X0 + 48]
        stp     x8,  x9,  [x0, #CTX_X0 + 64]
        stp     x10, x11, [x0, #CTX_X0 + 80]
        stp     x12, x13, [x0, #CTX_X0 + 96]
        stp     x14, x15, [x0, #CTX_X0 + 112]
        stp     x16, x17, [x0, #CTX_X0 + 128]
        stp     x18, x19, [x0, #CTX_X0 + 144]
        stp     x20, x21, [x0, #CTX_X0 + 160]
        stp     x22, x23, [x0, #CTX_X0 + 176]
        stp     x24, x25, [x0, #CTX_X0 + 192]
        stp     x26, x27, [x0, #CTX_X0 + 208]
        stp     x28, x29, [x0, #CTX_X0 + 224]
        str     x30,      [x0, #CTX_X30]

        /* Save stack pointers */
        mrs     x1, sp_el0
        str     x1,  [x0, #CTX_SP_EL0]
        mrs     x1, sp_el1
        str     x1,  [x0, #CTX_SP_EL1]

        /* Save EL1 exception return state */
        mrs     x1, elr_el1
        str     x1,  [x0, #CTX_ELR_EL1]
        mrs     x1, spsr_el1
        str     x1,  [x0, #CTX_SPSR_EL1]

        /* Save EL1 control registers */
        mrs     x1, sctlr_el1
        str     x1,  [x0, #CTX_SCTLR_EL1]
        mrs     x1, ttbr0_el1
        str     x1,  [x0, #CTX_TTBR0_EL1]
        mrs     x1, ttbr1_el1
        str     x1,  [x0, #CTX_TTBR1_EL1]
        mrs     x1, tcr_el1
        str     x1,  [x0, #CTX_TCR_EL1]
        mrs     x1, mair_el1
        str     x1,  [x0, #CTX_MAIR_EL1]
        mrs     x1, amair_el1
        str     x1,  [x0, #CTX_AMAIR_EL1]
        mrs     x1, vbar_el1
        str     x1,  [x0, #CTX_VBAR_EL1]
        mrs     x1, esr_el1
        str     x1,  [x0, #CTX_ESR_EL1]
        mrs     x1, far_el1
        str     x1,  [x0, #CTX_FAR_EL1]
        mrs     x1, afsr0_el1
        str     x1,  [x0, #CTX_AFSR0_EL1]
        mrs     x1, afsr1_el1
        str     x1,  [x0, #CTX_AFSR1_EL1]

        /* Save thread pointer registers */
        mrs     x1, tpidr_el0
        str     x1,  [x0, #CTX_TPIDR_EL0]
        mrs     x1, tpidr_el1
        str     x1,  [x0, #CTX_TPIDR_EL1]
        mrs     x1, tpidrro_el0
        str     x1,  [x0, #CTX_TPIDRRO_EL0]

        /* Save NEON/FP state: FPCR, FPSR, then Q0-Q31 as 16-byte stores.
         * CPTR_EL2.TFP must be 0 (trap disabled) so FP accesses at EL2
         * do not fault.  The caller (cpu_context.c) clears TFP on init. */
        mrs     x1, fpcr
        str     x1,  [x0, #CTX_FPCR]
        mrs     x1, fpsr
        str     x1,  [x0, #CTX_FPSR]
        stp     q0,  q1,  [x0, #CTX_Q0 + 0]
        stp     q2,  q3,  [x0, #CTX_Q0 + 32]
        stp     q4,  q5,  [x0, #CTX_Q0 + 64]
        stp     q6,  q7,  [x0, #CTX_Q0 + 96]
        stp     q8,  q9,  [x0, #CTX_Q0 + 128]
        stp     q10, q11, [x0, #CTX_Q0 + 160]
        stp     q12, q13, [x0, #CTX_Q0 + 192]
        stp     q14, q15, [x0, #CTX_Q0 + 224]
        stp     q16, q17, [x0, #CTX_Q0 + 256]
        stp     q18, q19, [x0, #CTX_Q0 + 288]
        stp     q20, q21, [x0, #CTX_Q0 + 320]
        stp     q22, q23, [x0, #CTX_Q0 + 352]
        stp     q24, q25, [x0, #CTX_Q0 + 384]
        stp     q26, q27, [x0, #CTX_Q0 + 416]
        stp     q28, q29, [x0, #CTX_Q0 + 448]
        stp     q30, q31, [x0, #CTX_Q0 + 480]

        /* Ensure all stores are visible before returning */
        dsb     sy
        ret


/* -----------------------------------------------------------------------
 * void hv_restore_guest_context(struct hv_cpu_context *ctx)
 *
 * Restores all EL1 system registers and GP registers from *ctx.
 * Programs VTTBR_EL2 with the context's VMID to activate the correct
 * stage-2 translation table before ERET to the guest.
 *
 * On entry: x0 = ctx pointer
 * Does NOT return - ends with ERET into the guest.
 * ----------------------------------------------------------------------- */

        .global hv_restore_guest_context
        .align  4
hv_restore_guest_context:
        /* Restore EL1 system registers first (before GP regs overwrite x0) */
        ldr     x1, [x0, #CTX_SCTLR_EL1]
        msr     sctlr_el1, x1
        ldr     x1, [x0, #CTX_TTBR0_EL1]
        msr     ttbr0_el1, x1
        ldr     x1, [x0, #CTX_TTBR1_EL1]
        msr     ttbr1_el1, x1
        ldr     x1, [x0, #CTX_TCR_EL1]
        msr     tcr_el1, x1
        ldr     x1, [x0, #CTX_MAIR_EL1]
        msr     mair_el1, x1
        ldr     x1, [x0, #CTX_AMAIR_EL1]
        msr     amair_el1, x1
        ldr     x1, [x0, #CTX_VBAR_EL1]
        msr     vbar_el1, x1
        ldr     x1, [x0, #CTX_AFSR0_EL1]
        msr     afsr0_el1, x1
        ldr     x1, [x0, #CTX_AFSR1_EL1]
        msr     afsr1_el1, x1
        ldr     x1, [x0, #CTX_TPIDR_EL0]
        msr     tpidr_el0, x1
        ldr     x1, [x0, #CTX_TPIDR_EL1]
        msr     tpidr_el1, x1
        ldr     x1, [x0, #CTX_TPIDRRO_EL0]
        msr     tpidrro_el0, x1

        /* Restore stack pointers */
        ldr     x1, [x0, #CTX_SP_EL0]
        msr     sp_el0, x1
        ldr     x1, [x0, #CTX_SP_EL1]
        msr     sp_el1, x1

        /* Restore exception return state */
        ldr     x1, [x0, #CTX_ELR_EL1]
        msr     elr_el2, x1          /* ERET will return to ELR_EL2 */
        ldr     x1, [x0, #CTX_SPSR_EL1]
        msr     spsr_el2, x1         /* SPSR_EL2 used by ERET */

        /* Restore VMID in VTTBR_EL2 while preserving current BADDR bits. */
        ldr     w1, [x0, #CTX_VMID]
        mrs     x2, vttbr_el2
        mov     x3, #0xffff
        lsl     x3, x3, #48
        bic     x2, x2, x3
        bfi     x2, x1, #48, #16
        msr     vttbr_el2, x2

        /* Restore NEON/FP state before GP registers overwrite x0 */
        ldp     q0,  q1,  [x0, #CTX_Q0 + 0]
        ldp     q2,  q3,  [x0, #CTX_Q0 + 32]
        ldp     q4,  q5,  [x0, #CTX_Q0 + 64]
        ldp     q6,  q7,  [x0, #CTX_Q0 + 96]
        ldp     q8,  q9,  [x0, #CTX_Q0 + 128]
        ldp     q10, q11, [x0, #CTX_Q0 + 160]
        ldp     q12, q13, [x0, #CTX_Q0 + 192]
        ldp     q14, q15, [x0, #CTX_Q0 + 224]
        ldp     q16, q17, [x0, #CTX_Q0 + 256]
        ldp     q18, q19, [x0, #CTX_Q0 + 288]
        ldp     q20, q21, [x0, #CTX_Q0 + 320]
        ldp     q22, q23, [x0, #CTX_Q0 + 352]
        ldp     q24, q25, [x0, #CTX_Q0 + 384]
        ldp     q26, q27, [x0, #CTX_Q0 + 416]
        ldp     q28, q29, [x0, #CTX_Q0 + 448]
        ldp     q30, q31, [x0, #CTX_Q0 + 480]
        ldr     x1, [x0, #CTX_FPCR]
        msr     fpcr, x1
        ldr     x1, [x0, #CTX_FPSR]
        msr     fpsr, x1

        /* ISB to ensure all sysreg writes complete before GP registers load */
        isb

        /* Restore general-purpose registers */
        ldr     x30, [x0, #CTX_X30]
        ldp     x28, x29, [x0, #CTX_X0 + 224]
        ldp     x26, x27, [x0, #CTX_X0 + 208]
        ldp     x24, x25, [x0, #CTX_X0 + 192]
        ldp     x22, x23, [x0, #CTX_X0 + 176]
        ldp     x20, x21, [x0, #CTX_X0 + 160]
        ldp     x18, x19, [x0, #CTX_X0 + 144]
        ldp     x16, x17, [x0, #CTX_X0 + 128]
        ldp     x14, x15, [x0, #CTX_X0 + 112]
        ldp     x12, x13, [x0, #CTX_X0 + 96]
        ldp     x10, x11, [x0, #CTX_X0 + 80]
        ldp     x8,  x9,  [x0, #CTX_X0 + 64]
        ldp     x6,  x7,  [x0, #CTX_X0 + 48]
        ldp     x4,  x5,  [x0, #CTX_X0 + 32]
        ldp     x2,  x3,  [x0, #CTX_X0 + 16]
        ldp     x0,  x1,  [x0, #CTX_X0 + 0]   /* x0 last - overwrites ctx ptr */

        eret                 /* Return to guest at ELR_EL2 with SPSR_EL2 */


/* -----------------------------------------------------------------------
 * void hv_context_switch(struct hv_cpu_context *save,
 *                        struct hv_cpu_context *restore,
 *                        uint64_t vttbr)
 *
 * Used during partition scheduling: save running partition context,
 * switch VTTBR_EL2 (VMID + stage-2 table base), restore next partition.
 *
 * x0 = save context pointer
 * x1 = restore context pointer
 * x2 = new VTTBR_EL2 value (encodes VMID + BADDR)
 * ----------------------------------------------------------------------- */

        .global hv_context_switch
        .align  4
hv_context_switch:
        /* Save current partition's GP registers into *x0 */
        stp     x0,  x1,  [x0, #CTX_X0 + 0]
        stp     x2,  x3,  [x0, #CTX_X0 + 16]
        stp     x4,  x5,  [x0, #CTX_X0 + 32]
        stp     x6,  x7,  [x0, #CTX_X0 + 48]
        stp     x8,  x9,  [x0, #CTX_X0 + 64]
        stp     x10, x11, [x0, #CTX_X0 + 80]
        stp     x12, x13, [x0, #CTX_X0 + 96]
        stp     x14, x15, [x0, #CTX_X0 + 112]
        stp     x16, x17, [x0, #CTX_X0 + 128]
        stp     x18, x19, [x0, #CTX_X0 + 144]
        stp     x20, x21, [x0, #CTX_X0 + 160]
        stp     x22, x23, [x0, #CTX_X0 + 176]
        stp     x24, x25, [x0, #CTX_X0 + 192]
        stp     x26, x27, [x0, #CTX_X0 + 208]
        stp     x28, x29, [x0, #CTX_X0 + 224]
        str     x30,      [x0, #CTX_X30]
        mrs     x3, sp_el0
        str     x3,  [x0, #CTX_SP_EL0]
        mrs     x3, sp_el1
        str     x3,  [x0, #CTX_SP_EL1]
        mrs     x3, elr_el1
        str     x3,  [x0, #CTX_ELR_EL1]
        mrs     x3, spsr_el1
        str     x3,  [x0, #CTX_SPSR_EL1]
        mrs     x3, sctlr_el1
        str     x3,  [x0, #CTX_SCTLR_EL1]
        mrs     x3, ttbr0_el1
        str     x3,  [x0, #CTX_TTBR0_EL1]
        mrs     x3, ttbr1_el1
        str     x3,  [x0, #CTX_TTBR1_EL1]
        mrs     x3, tcr_el1
        str     x3,  [x0, #CTX_TCR_EL1]
        mrs     x3, mair_el1
        str     x3,  [x0, #CTX_MAIR_EL1]
        mrs     x3, tpidr_el0
        str     x3,  [x0, #CTX_TPIDR_EL0]
        mrs     x3, tpidr_el1
        str     x3,  [x0, #CTX_TPIDR_EL1]
        mrs     x3, tpidrro_el0
        str     x3,  [x0, #CTX_TPIDRRO_EL0]

        /* Save NEON/FP state for the outgoing partition */
        mrs     x3, fpcr
        str     x3,  [x0, #CTX_FPCR]
        mrs     x3, fpsr
        str     x3,  [x0, #CTX_FPSR]
        stp     q0,  q1,  [x0, #CTX_Q0 + 0]
        stp     q2,  q3,  [x0, #CTX_Q0 + 32]
        stp     q4,  q5,  [x0, #CTX_Q0 + 64]
        stp     q6,  q7,  [x0, #CTX_Q0 + 96]
        stp     q8,  q9,  [x0, #CTX_Q0 + 128]
        stp     q10, q11, [x0, #CTX_Q0 + 160]
        stp     q12, q13, [x0, #CTX_Q0 + 192]
        stp     q14, q15, [x0, #CTX_Q0 + 224]
        stp     q16, q17, [x0, #CTX_Q0 + 256]
        stp     q18, q19, [x0, #CTX_Q0 + 288]
        stp     q20, q21, [x0, #CTX_Q0 + 320]
        stp     q22, q23, [x0, #CTX_Q0 + 352]
        stp     q24, q25, [x0, #CTX_Q0 + 384]
        stp     q26, q27, [x0, #CTX_Q0 + 416]
        stp     q28, q29, [x0, #CTX_Q0 + 448]
        stp     q30, q31, [x0, #CTX_Q0 + 480]

        /* Switch stage-2 translation table (VMID change) */
        dsb     sy
        msr     vttbr_el2, x2        /* x2 = new VTTBR from caller */
        tlbi    vmalls12e1is         /* invalidate all stage-2 TLBs for old VMID */
        dsb     ish
        isb

        /* Fall through: restore new partition's context from *x1 */
        mov     x0, x1
        b       hv_restore_guest_context   /* tail call - ends with ERET */
