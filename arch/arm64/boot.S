/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/boot.S — Haven hypervisor primary boot entry.
 *
 * Firmware (U-Boot / ATF) loads haven.bin at HV_LOAD_ADDR and jumps here
 * at EL2 with:
 *   x0 = physical address of DTB (device tree blob)
 *   x1 = 0 (reserved)
 *   x2 = 0 (reserved)
 *   x3 = 0 (reserved)
 *
 * The primary CPU sets up:
 *   - EL2 stack pointer
 *   - Zero BSS segment
 *   - Calls haven_init(dtb_pa)
 *
 * Secondary CPUs spin on a mailbox word until the primary CPU fills in
 * their entry point via hv_arch_secondary_start().
 *
 * Note: adrp+add is used throughout instead of adr because the total image
 * (including the 4MB page-table pool in BSS) exceeds the ±1MB adr reach.
 */

        .section ".entry.text", "ax"

/* -----------------------------------------------------------------------
 * Build-time constants (define before first use)
 * ----------------------------------------------------------------------- */
#ifndef HV_MAX_CPUS
#define HV_MAX_CPUS     8
#endif
#ifndef HV_STACK_SIZE
#define HV_STACK_SIZE   4096
#endif

/* -----------------------------------------------------------------------
 * Primary CPU entry point — symbol must match linker script entry
 * ----------------------------------------------------------------------- */

        .global _start
        .align  3
_start:
        /* Verify we are at EL2 (bits [3:2] of CurrentEL) */
        mrs     x4, CurrentEL
        lsr     x4, x4, #2
        and     x4, x4, #3
        cmp     x4, #2
        b.ne    _el_wrong        /* Halt if not EL2 */

        /* Set up EL2 stack: cpu0 uses the top of the first stack slot.
         * adrp gives the 4KB-page address; add supplies the page offset. */
        adrp    x4, cpu_stacks
        add     x4, x4, :lo12:cpu_stacks
        add     x4, x4, #(HV_STACK_SIZE)   /* CPU 0 stack top */
        mov     sp, x4

        /* Zero BSS: from __bss_start to __bss_end (8-byte aligned) */
        adrp    x4, __bss_start
        add     x4, x4, :lo12:__bss_start
        adrp    x5, __bss_end
        add     x5, x5, :lo12:__bss_end
_bss_zero:
        cmp     x4, x5
        b.ge    _bss_done
        str     xzr, [x4], #8
        b       _bss_zero
_bss_done:

        /* Initialize secondary CPU mailbox words to ~0 (parked state) */
        adrp    x4, secondary_entry_addrs
        add     x4, x4, :lo12:secondary_entry_addrs
        mov     x5, #HV_MAX_CPUS
        mov     x6, #0xffffffffffffffff
_mailbox_init:
        cbz     x5, _mailbox_done
        str     x6, [x4], #8
        sub     x5, x5, #1
        b       _mailbox_init
_mailbox_done:

        /* x0 still holds DTB physical address from firmware — pass to C */
        bl      haven_init

        /* haven_init should never return; if it does, halt */
        b       _halt


/* -----------------------------------------------------------------------
 * Secondary CPU entry — jumped to after PSCI CPU_ON
 *
 * Secondary CPUs spin here polling their mailbox word until the primary
 * CPU has initialized the hypervisor and provides an entry address.
 * ----------------------------------------------------------------------- */

        .global secondary_entry
        .align  3
secondary_entry:
        /* Read MPIDR to get CPU logical index */
        mrs     x0, mpidr_el1
        and     x0, x0, #0xff     /* Aff0 (CPU within cluster) */

        /* Use Aff0 as stack index (up to HV_MAX_CPUS per cluster) */
        adrp    x1, cpu_stacks
        add     x1, x1, :lo12:cpu_stacks
        mov     x2, #HV_STACK_SIZE
        mul     x3, x0, x2
        add     x1, x1, x3
        add     x1, x1, x2        /* Top of this CPU's stack */
        mov     sp, x1

        /* Spin until primary CPU provides entry address in mailbox */
        adrp    x1, secondary_entry_addrs
        add     x1, x1, :lo12:secondary_entry_addrs
        add     x1, x1, x0, lsl #3   /* &secondary_entry_addrs[cpu_id] */
_secondary_spin:
        ldr     x2, [x1]
        /* cmn x2, #1 sets Z when x2 == -1 (all bits set sentinel). */
        cmn     x2, #1
        b.eq    _secondary_spin
        dsb     sy
        isb

        /* Jump to provided entry with x0 = cpu_id */
        br      x2


/* -----------------------------------------------------------------------
 * hv_arch_secondary_start — wake a secondary CPU.
 *
 * Called by primary after hypervisor init to release secondary CPUs.
 * cpu_id:   logical CPU index
 * entry_fn: function for secondary CPU to call (takes cpu_id as arg)
 * ----------------------------------------------------------------------- */

        .global hv_arch_secondary_start
        .align  3
hv_arch_secondary_start:
        /* x0 = cpu_id, x1 = entry_fn */
        adrp    x2, secondary_entry_addrs
        add     x2, x2, :lo12:secondary_entry_addrs
        str     x1, [x2, x0, lsl #3]
        dsb     sy
        /* Send event to wake secondary CPUs from WFE */
        sev
        ret


/* -----------------------------------------------------------------------
 * Error paths
 * ----------------------------------------------------------------------- */

        .align  3
_el_wrong:
        /* Haven loaded at wrong exception level — spin forever */
1:      wfi
        b       1b

        .align  3
_halt:
1:      wfi
        b       1b


/* -----------------------------------------------------------------------
 * Data: per-CPU stacks and secondary mailbox
 * Placed in BSS (zero-initialized).
 * ----------------------------------------------------------------------- */

        .section ".bss", "aw"
        .align  4

        .global cpu_stacks
cpu_stacks:
        .space  (HV_MAX_CPUS * HV_STACK_SIZE)

        .global secondary_entry_addrs
secondary_entry_addrs:
        .space  (HV_MAX_CPUS * 8)
