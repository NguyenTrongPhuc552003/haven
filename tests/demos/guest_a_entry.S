/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_a_entry.S
 *
 * Minimal bare-metal EL1 stub for Partition A.
 *
 * Haven ERETSs into this code at IPA 0x40000000 with:
 *   SPSR_EL2 = 0x3C5  (EL1h, all DAIF masked)
 *   ELR_EL2  = 0x40000000
 *   SP_EL1   set to top of partition A IPA space
 *
 * What this stub does:
 *   1. Initialises the EL1 stack.
 *   2. Prints "PARTITION_A: Hello from partition A (EL1 guest)\r\n" via PL011.
 *   3. Reads from IPA 0x60000000 (Partition B's IPA base) - this must trigger
 *      a stage-2 fault which Haven catches, logs, skips, and returns here.
 *   4. Prints "PARTITION_A: Post-fault check OK\r\n" to confirm Haven handled
 *      the fault and resumed execution.
 *   5. Halts with WFI loop.
 *
 * PL011 UART at IPA 0x09000000 (identity-mapped from partition.c).
 * PL011 data register is at offset 0x00; busy flag at UARTFR (offset 0x18)
 * bit 5 (TXFF).  We poll TXFF before each byte to avoid overrun.
 */

#define UART_BASE    0x09000000
#define UART_DR      0x00   /* Data Register */
#define UART_FR      0x18   /* Flag Register */
#define UART_FR_TXFF (1 << 5)

/* Stack at top of partition A IPA DRAM (passed as SP_EL1 by haven) */

        .section ".text.entry", "ax"
        .global _guest_start
        .type   _guest_start, %function

_guest_start:
        /* Set up a local stack in case the linker SP_EL1 is 0 */
        adr     x0, _guest_stack_top
        mov     sp, x0

        /* Print greeting */
        adr     x0, msg_hello
        bl      uart_puts

        /* Deliberate cross-partition access: read from Partition B IPA */
        mov     x1, #0x6000
        lsl     x1, x1, #16        /* x1 = 0x60000000 (PART_B_IPA_BASE) */
        ldr     x0, [x1]           /* stage-2 fault -> Haven logs + skips */

        /* If Haven advanced ELR_EL2 by 4 we resume here */
        adr     x0, msg_post_fault
        bl      uart_puts

_guest_halt:
        wfi
        b       _guest_halt

/* -----------------------------------------------------------------------
 * uart_puts(x0 = pointer to NUL-terminated string)
 * Clobbers: x0, x1, x2, x3
 * ----------------------------------------------------------------------- */
uart_puts:
        mov     x2, x0
        mov     x3, #UART_BASE
1:
        ldrb    w0, [x2], #1
        cbz     w0, 2f
        /* Poll TXFF until transmit FIFO has space */
3:      ldr     w1, [x3, #UART_FR]
        tst     w1, #UART_FR_TXFF
        b.ne    3b
        str     w0, [x3, #UART_DR]
        b       1b
2:
        ret

/* -----------------------------------------------------------------------
 * Strings
 * ----------------------------------------------------------------------- */
        .section ".rodata", "a"
msg_hello:
        .asciz  "PARTITION_A: Hello from partition A (EL1 guest)\r\n"
msg_post_fault:
        .asciz  "PARTITION_A: Post-fault check OK\r\n"

/* -----------------------------------------------------------------------
 * 4 KB stack for the guest stub
 * ----------------------------------------------------------------------- */
        .section ".bss.stack", "aw"
        .align  12
_guest_stack:
        .space  4096
_guest_stack_top:
