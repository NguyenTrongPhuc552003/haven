/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_b_entry.S
 *
 * Minimal bare-metal EL1 stub for Partition B (RTOS-class guest).
 *
 * Haven ERETSs into this code at IPA 0x60000000 (PART_B_IPA_BASE) with:
 *   SPSR_EL2 = 0x3C5  (EL1h, all DAIF masked, AArch64)
 *   ELR_EL2  = 0x60000000
 *   SP_EL1   = top of partition B IPA space
 *
 * Partition B has NO UART MMIO mapped in its stage-2 page table (the PL011
 * at 0x09000000 is exclusively owned by Partition A).  All per-partition
 * activity is visible through Haven's own serial output:
 *
 *   "HAVEN: partition B mapped PA=..."  -- emitted during haven_init()
 *   "HAVEN: Denied stage-2 violation IPA=0x40000000 from cpu=2"
 *                                       -- when B reads from A's IPA
 *
 * What this stub demonstrates:
 *   1. Sets up EL1 stack and runs 3 simulated RTOS task periods (busy loops).
 *   2. Deliberately reads IPA 0x40000000 (Partition A's IPA base) to prove
 *      cross-partition memory is blocked: Haven catches the stage-2 DABT,
 *      logs the denial, and advances ELR_EL2 by 4 so execution resumes.
 *   3. Writes a "done" sentinel (0xDEADBEEF) to offset 0x100 in its own
 *      IPA space so Haven can verify B completed via a stage-2 read.
 *   4. Halts with a WFI loop.
 *
 * No UART for B preserves the spatial isolation invariant: Partition B
 * cannot read or write any PA owned by Partition A (including UART MMIO).
 */

/* Simulated RTOS "task" workload: spin 2^20 iterations per period. */
#define TASK_SPIN_ITERS  0x100000

        .section ".text.entry", "ax"
        .global _guest_b_start
        .type   _guest_b_start, %function

_guest_b_start:
        /* Set up EL1 stack. */
        adr     x0, _guest_b_stack_top
        mov     sp, x0

        /* --- RTOS period 1 ------------------------------------------- */
        bl      rtos_task_body

        /* --- RTOS period 2 ------------------------------------------- */
        bl      rtos_task_body

        /* --- RTOS period 3 ------------------------------------------- */
        bl      rtos_task_body

        /* Deliberate cross-partition access: read from Partition A's IPA.
         * This triggers a stage-2 DABT (FSC=0x04, EC=0x24).
         * Haven logs "HAVEN: Denied stage-2 violation IPA=0x40000000"
         * and advances ELR_EL2 by 4, resuming execution at the next instr. */
        mov     x1, #0x4000
        lsl     x1, x1, #16             /* x1 = 0x40000000 = PART_A_IPA_BASE */
        ldr     x0, [x1]               /* -> stage-2 fault, Haven skips and resumes */

        /* Write done sentinel to own IPA base + 0x100.
         * A future health-check pass in Haven can read this back via
         * hv_stage2_read() to confirm Partition B executed successfully. */
        mov     x2, #0x6000
        lsl     x2, x2, #16             /* x2 = 0x60000000 = PART_B_IPA_BASE */
        mov     w3, #0xBEEF
        movk    w3, #0xDEAD, lsl #16    /* w3 = 0xDEADBEEF */
        str     w3, [x2, #0x100]

_guest_b_halt:
        wfi
        b       _guest_b_halt

/* -----------------------------------------------------------------------
 * rtos_task_body - simulated 1-period RTOS task (busy-wait workload)
 * Clobbers: x0
 * ----------------------------------------------------------------------- */
rtos_task_body:
        mov     x0, #TASK_SPIN_ITERS
1:      subs    x0, x0, #1
        b.ne    1b
        ret

/* -----------------------------------------------------------------------
 * 4 KB stack for partition B guest stub
 * ----------------------------------------------------------------------- */
        .section ".bss.stack", "aw"
        .align  12
_guest_b_stack:
        .space  4096
_guest_b_stack_top:
