/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_a_vectors.S - EL1 exception vectors for partition A.
 *
 * Only "current EL with SP_ELx, IRQ" (offset 0x280) is handled: it saves the
 * caller-saved registers, calls guest_a_irq_handler() and returns.  Every
 * other entry parks the CPU, which a test sees as a missing UART line.
 */

        .macro  park
        .align  7
1:      wfe
        b       1b
        .endm

        .section ".text", "ax"
        .align  11
        .global guest_a_vectors
guest_a_vectors:
        park                    /* 0x000 current EL, SP_EL0: sync */
        park                    /* 0x080 IRQ */
        park                    /* 0x100 FIQ */
        park                    /* 0x180 SError */
        park                    /* 0x200 current EL, SP_ELx: sync */

        .align  7               /* 0x280 current EL, SP_ELx: IRQ */
        sub     sp, sp, #176
        stp     x0, x1, [sp, #0]
        stp     x2, x3, [sp, #16]
        stp     x4, x5, [sp, #32]
        stp     x6, x7, [sp, #48]
        stp     x8, x9, [sp, #64]
        stp     x10, x11, [sp, #80]
        stp     x12, x13, [sp, #96]
        stp     x14, x15, [sp, #112]
        stp     x16, x17, [sp, #128]
        stp     x18, x29, [sp, #144]
        str     x30, [sp, #160]
        bl      guest_a_irq_handler
        ldr     x30, [sp, #160]
        ldp     x18, x29, [sp, #144]
        ldp     x16, x17, [sp, #128]
        ldp     x14, x15, [sp, #112]
        ldp     x12, x13, [sp, #96]
        ldp     x10, x11, [sp, #80]
        ldp     x8, x9, [sp, #64]
        ldp     x6, x7, [sp, #48]
        ldp     x4, x5, [sp, #32]
        ldp     x2, x3, [sp, #16]
        ldp     x0, x1, [sp, #0]
        add     sp, sp, #176
        eret

        park                    /* 0x300 FIQ */
        park                    /* 0x380 SError */
        park                    /* 0x400 lower EL, AArch64 */
        park                    /* 0x480 */
        park                    /* 0x500 */
        park                    /* 0x580 */
        park                    /* 0x600 lower EL, AArch32 */
        park                    /* 0x680 */
        park                    /* 0x700 */
        park                    /* 0x780 */
