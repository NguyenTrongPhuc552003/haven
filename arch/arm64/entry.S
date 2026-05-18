/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/entry.S — EL2 exception vector table for the Haven hypervisor.
 *
 * ARM Architecture Reference Manual requires VBAR_EL2 to be aligned
 * to a 2KiB boundary (bits [10:0] = 0).  The table contains 16 entries,
 * each 128 bytes (32 instructions).  Layout:
 *
 *   +0x000  SP0 / Sync
 *   +0x080  SP0 / IRQ
 *   +0x100  SP0 / FIQ
 *   +0x180  SP0 / SError
 *   +0x200  SPx / Sync        ← Haven EL2 faults (shouldn't normally fire)
 *   +0x280  SPx / IRQ
 *   +0x300  SPx / FIQ
 *   +0x380  SPx / SError
 *   +0x400  Lower AArch64 / Sync   ← Guest traps (main work)
 *   +0x480  Lower AArch64 / IRQ
 *   +0x500  Lower AArch64 / FIQ
 *   +0x580  Lower AArch64 / SError
 *   +0x600  Lower AArch32 / Sync   (not used; Haven only runs AArch64 guests)
 *   +0x680  Lower AArch32 / IRQ
 *   +0x700  Lower AArch32 / FIQ
 *   +0x780  Lower AArch32 / SError
 */

/* Haven kernel stack per CPU: allocated in BSS by linker script */
#define HV_STACK_SIZE   4096
#define HV_CONTEXT_SIZE 272  /* sizeof(struct hv_cpu_context): see context.h */

/*
 * vector_entry — emit a 128-byte exception vector entry.
 * The entry saves x0/x1, branches to the handler, and is padded to
 * exactly 128 bytes so the vector table alignment is maintained.
 */
.macro vector_entry label
        .align 7                /* 128-byte alignment per entry */
        b       \label
.endm

/* -----------------------------------------------------------------------
 * EL2 exception vector table — must be .align 11 (2 KiB boundary)
 * ----------------------------------------------------------------------- */

        .section ".entry.text", "ax"
        .global el2_exception_table
        .align  11

el2_exception_table:

        /* ---- EL2 with SP0 (should never happen in Haven) ---- */
        vector_entry el2_sp0_sync
        vector_entry el2_sp0_irq
        vector_entry el2_sp0_fiq
        vector_entry el2_sp0_serror

        /* ---- EL2 with SP_EL2 (Haven itself faulted) ---- */
        vector_entry el2_spx_sync
        vector_entry el2_spx_irq
        vector_entry el2_spx_fiq
        vector_entry el2_spx_serror

        /* ---- Lower EL AArch64 (guest traps — main work) ---- */
        vector_entry lower_el64_sync
        vector_entry lower_el64_irq
        vector_entry lower_el64_fiq
        vector_entry lower_el64_serror

        /* ---- Lower EL AArch32 (not used; abort immediately) ---- */
        vector_entry lower_el32_sync
        vector_entry lower_el32_irq
        vector_entry lower_el32_fiq
        vector_entry lower_el32_serror


/* -----------------------------------------------------------------------
 * Save / restore macros
 *
 * hv_save_minimal: saves caller-saved registers x0–x18, LR, ELR_EL2,
 * SPSR_EL2 onto the EL2 stack and calls C handler(esr, far, ctx).
 * We keep this minimal to keep exception latency bounded.
 * ----------------------------------------------------------------------- */

/*
 * save_minimal — push the minimum needed to call a C handler.
 * Layout on stack (SP decreases, low address at top after push):
 *   [sp+0]   x0
 *   [sp+8]   x1
 *   [sp+16]  x2
 *   [sp+24]  x3
 *   [sp+32]  x4
 *   [sp+40]  x5
 *   [sp+48]  x6
 *   [sp+56]  x7
 *   [sp+64]  x8
 *   [sp+72]  x9
 *   [sp+80]  x10
 *   [sp+88]  x11
 *   [sp+96]  x12
 *   [sp+104] x13
 *   [sp+112] x14
 *   [sp+120] x15
 *   [sp+128] x16
 *   [sp+136] x17
 *   [sp+144] x18
 *   [sp+152] x29 (frame pointer)
 *   [sp+160] x30 (LR)
 *   [sp+168] ELR_EL2
 *   [sp+176] SPSR_EL2
 *   Total: 184 bytes
 */
.macro save_minimal
        sub     sp, sp, #192
        stp     x0,  x1,  [sp, #0]
        stp     x2,  x3,  [sp, #16]
        stp     x4,  x5,  [sp, #32]
        stp     x6,  x7,  [sp, #48]
        stp     x8,  x9,  [sp, #64]
        stp     x10, x11, [sp, #80]
        stp     x12, x13, [sp, #96]
        stp     x14, x15, [sp, #112]
        stp     x16, x17, [sp, #128]
        stp     x18, x29, [sp, #144]
        stp     x30, xzr, [sp, #160]
        mrs     x0, elr_el2
        mrs     x1, spsr_el2
        stp     x0,  x1,  [sp, #168]
.endm

.macro restore_minimal
        ldp     x0,  x1,  [sp, #168]
        msr     elr_el2,  x0
        msr     spsr_el2, x1
        ldp     x30, xzr, [sp, #160]
        ldp     x18, x29, [sp, #144]
        ldp     x16, x17, [sp, #128]
        ldp     x14, x15, [sp, #112]
        ldp     x12, x13, [sp, #96]
        ldp     x10, x11, [sp, #80]
        ldp     x8,  x9,  [sp, #64]
        ldp     x6,  x7,  [sp, #48]
        ldp     x4,  x5,  [sp, #32]
        ldp     x2,  x3,  [sp, #16]
        ldp     x0,  x1,  [sp, #0]
        add     sp, sp, #192
.endm


/* -----------------------------------------------------------------------
 * Lower EL AArch64 handlers — guest trap paths
 * ----------------------------------------------------------------------- */

        .align 4
lower_el64_sync:
        save_minimal
        mrs     x0, esr_el2       /* arg0: exception syndrome */
        mrs     x1, far_el2       /* arg1: fault address */
        mov     x2, sp            /* arg2: pointer to saved frame */
        bl      hv_handle_sync    /* C handler in el2_exceptions.c */
        restore_minimal
        eret

        .align 4
lower_el64_irq:
        save_minimal
        mrs     x0, esr_el2
        mov     x1, #0            /* FAR not valid for IRQ */
        mov     x2, sp
        bl      hv_handle_irq     /* C handler */
        restore_minimal
        eret

        .align 4
lower_el64_fiq:
        save_minimal
        mrs     x0, esr_el2
        mov     x1, #0
        mov     x2, sp
        bl      hv_handle_fiq     /* C handler */
        restore_minimal
        eret

        .align 4
lower_el64_serror:
        save_minimal
        mrs     x0, esr_el2
        mrs     x1, far_el2
        mov     x2, sp
        bl      hv_handle_serror  /* C handler */
        restore_minimal
        eret


/* -----------------------------------------------------------------------
 * EL2 SPx handlers — Haven itself faulted (fatal)
 * These should never fire in a correct implementation.
 * ----------------------------------------------------------------------- */

        .align 4
el2_spx_sync:
        mrs     x0, esr_el2
        mrs     x1, far_el2
        mrs     x2, elr_el2
        bl      hv_fatal_exception  /* does not return */
1:      wfi
        b       1b

        .align 4
el2_spx_irq:
        bl      hv_fatal_exception
1:      wfi
        b       1b

        .align 4
el2_spx_fiq:
        bl      hv_fatal_exception
1:      wfi
        b       1b

        .align 4
el2_spx_serror:
        bl      hv_fatal_exception
1:      wfi
        b       1b


/* -----------------------------------------------------------------------
 * EL2 SP0 handlers (should never fire)
 * ----------------------------------------------------------------------- */

        .align 4
el2_sp0_sync:
el2_sp0_irq:
el2_sp0_fiq:
el2_sp0_serror:
        bl      hv_fatal_exception
1:      wfi
        b       1b


/* -----------------------------------------------------------------------
 * Lower EL AArch32 handlers (not supported — abort)
 * ----------------------------------------------------------------------- */

        .align 4
lower_el32_sync:
lower_el32_irq:
lower_el32_fiq:
lower_el32_serror:
        /* AArch32 guests are not supported; treat as fatal */
        bl      hv_fatal_exception
1:      wfi
        b       1b


/* -----------------------------------------------------------------------
 * hv_install_vectors — write VBAR_EL2 and synchronize
 * Called once during CPU initialization.
 * ----------------------------------------------------------------------- */

        .global hv_install_vectors
        .align 4
hv_install_vectors:
        adr     x0, el2_exception_table
        msr     vbar_el2, x0
        isb
        ret
