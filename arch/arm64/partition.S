/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/partition.S — Haven EL2-to-EL1 partition launch stub.
 *
 * hv_arch_start_partition performs ERET from EL2 to EL1h, transferring
 * control to a guest partition's entry point.  It does NOT return.
 *
 * Note: sysregs.h is intentionally not included here.  That header
 * contains C typedef declarations which cannot be parsed by the
 * assembler-with-cpp pipeline used for .S files in this project.
 * All instructions below are written as raw assembly mnemonics.
 */

        .global hv_arch_start_partition
        .type   hv_arch_start_partition, %function

/*
 * hv_arch_start_partition(uintptr_t entry, uintptr_t sp, uintptr_t arg)
 *
 *   x0 = entry point physical address (first instruction for the guest)
 *   x1 = initial EL1 stack pointer
 *   x2 = argument passed to the guest in x0 (typically a DTB pointer)
 *
 * Execution model:
 *   SPSR_EL2  = EL1h | DAIF fully masked so the guest starts with a clean
 *               interrupt state and can unmask selectively.
 *   ELR_EL2   = entry  (ERET branches to this address)
 *   SP_EL1    = sp     (guest sees this as its stack pointer at EL1)
 *   x0        = arg    (first argument register for the guest entry)
 *   x1-x3     = 0      (clean general-purpose state)
 */

hv_arch_start_partition:
        /*
         * Build SPSR_EL2 value:
         *   bits [3:0] = 0x05  EL1h (EL1 with SP_EL1 selected)
         *   bits [9:6] = 0xF   DAIF all masked (D, A, I, F = 1)
         *   bit  [4]   = 0     nRW = 0 → AArch64 mode
         *
         *   0x05 | (0xF << 6) = 0x05 | 0x3C0 = 0x3C5
         */
        mov     x3, #0x3C5
        msr     spsr_el2, x3

        /* ELR_EL2 → guest entry point (ERET will branch here) */
        msr     elr_el2, x0

        /* SP_EL1 → initial guest stack pointer */
        msr     sp_el1, x1

        /* Pass arg (e.g. DTB PA) in x0 as the guest's first argument */
        mov     x0, x2

        /* Clear x1-x3 to give the guest a deterministic register state */
        mov     x1, #0
        mov     x2, #0
        mov     x3, #0

        /* Serialise all preceding register writes before ERET */
        isb

        /* Transfer control to EL1 — does not return */
        eret

        .size hv_arch_start_partition, . - hv_arch_start_partition
