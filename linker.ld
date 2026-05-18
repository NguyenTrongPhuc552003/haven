/* SPDX-License-Identifier: Apache-2.0 */
/*
 * linker.ld — ELF linker script for the Haven hypervisor (ARM64).
 *
 * Haven is loaded at 0x80000000 by firmware (U-Boot / ATF) on the
 * primary platform targets:
 *   QEMU virt:    -bios haven.bin → loaded at QEMU EL2 load address
 *   i.MX95:       U-Boot loads haven.bin at configured address
 *
 * The physical load address can be overridden by passing
 *   LD_FLAGS += --defsym=HV_LOAD_ADDR=0xXXXXXXXX
 * on the linker command line (e.g. for i.MX95 DDR at 0x80000000 + offset).
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

ENTRY(_start)

/* Default load address; override with LD_FLAGS if needed */
HV_LOAD_ADDR = DEFINED(HV_LOAD_ADDR) ? HV_LOAD_ADDR : 0x80000000;

SECTIONS
{
        . = HV_LOAD_ADDR;

        /* ----------------------------------------------------------------
         * .entry.text — exception vectors and boot stub.
         * Must be placed before .text so VBAR_EL2 = HV_LOAD_ADDR
         * is correctly aligned (2KiB for VBAR_EL2, but we align 4KiB
         * for simplicity).
         * ---------------------------------------------------------------- */
        .entry.text : ALIGN(4096)
        {
                *(.entry.text)
        }

        /* ----------------------------------------------------------------
         * .text — all other code
         * ---------------------------------------------------------------- */
        .text : ALIGN(64)
        {
                *(.text .text.*)
        }

        /* ----------------------------------------------------------------
         * .rodata — read-only data (string literals, const arrays)
         * ---------------------------------------------------------------- */
        .rodata : ALIGN(64)
        {
                *(.rodata .rodata.*)
        }

        /* ----------------------------------------------------------------
         * .data — initialized read-write data
         * ---------------------------------------------------------------- */
        .data : ALIGN(64)
        {
                *(.data .data.*)
        }

        /* ----------------------------------------------------------------
         * .bss — zero-initialized data (stacks, page-table pool, etc.)
         * The boot stub zeroes this region before calling haven_init().
         * ---------------------------------------------------------------- */
        . = ALIGN(4096);
        __bss_start = .;

        .bss : ALIGN(4096)
        {
                *(.bss .bss.*)
                *(COMMON)
        }

        . = ALIGN(8);
        __bss_end = .;

        /* ----------------------------------------------------------------
         * Discard DWARF sections in production build (reduce binary size).
         * Comment this out to enable GDB debugging.
         * ---------------------------------------------------------------- */
        /DISCARD/ :
        {
                *(.comment)
                *(.note.*)
        }
}

/* -----------------------------------------------------------------------
 * Exported symbols used by boot.S
 * ----------------------------------------------------------------------- */
PROVIDE(__bss_start = __bss_start);
PROVIDE(__bss_end   = __bss_end);
