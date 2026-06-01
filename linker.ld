/* SPDX-License-Identifier: Apache-2.0 */
/*
 * linker.ld - ELF linker script for the Haven hypervisor (ARM64).
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
         * .boot.text - primary and secondary CPU boot stub (_start).
         * MUST be placed first so that HV_LOAD_ADDR == _start and
         * QEMU/firmware jumps directly into the boot stub.
         * boot.S places its code here exclusively so that no filename
         * glob is needed (avoids CMake .obj vs .o ambiguity).
         * ---------------------------------------------------------------- */
        .boot.text : ALIGN(4096)
        {
                KEEP(*(.boot.text))
        }

        /* ----------------------------------------------------------------
         * .entry.text - EL2 exception vector table (el2_exception_table).
         * VBAR_EL2 is set to &el2_exception_table via adr (PC-relative)
         * in hv_install_vectors, so the absolute address does not matter
         * as long as it is 2KiB-aligned (.align 11 in entry.S).
         * ---------------------------------------------------------------- */
        .entry.text : ALIGN(2048)
        {
                *(.entry.text)
        }

        /* ----------------------------------------------------------------
         * .text - all other code
         * ---------------------------------------------------------------- */
        .text : ALIGN(64)
        {
                *(.text .text.*)
        }

        /* ----------------------------------------------------------------
         * .rodata - read-only data (string literals, const arrays)
         * ---------------------------------------------------------------- */
        .rodata : ALIGN(64)
        {
                *(.rodata .rodata.*)
        }

        /* ----------------------------------------------------------------
         * .data - initialized read-write data
         * ---------------------------------------------------------------- */
        .data : ALIGN(64)
        {
                *(.data .data.*)
        }

        /* ----------------------------------------------------------------
         * .bss - zero-initialized data (stacks, page-table pool, etc.)
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
