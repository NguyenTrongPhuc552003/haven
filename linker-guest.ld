/*
 * linker-guest.ld - Guest A stub linker script.
 *
 * Links the guest_a_entry.S stub at VMA 0x40000000 (Partition A IPA base).
 * Haven loads the resulting binary at PA 0x80800000 and stage-2 maps
 * PA 0x80800000 → IPA 0x40000000, so the guest executes at its VMA.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

ENTRY(_guest_start)

SECTIONS
{
    . = 0x40000000;

    .text.entry : { *(.text.entry) }
    .text       : { *(.text*) }
    .rodata     : { *(.rodata*) }
    .data       : { *(.data*) }

    . = ALIGN(4096);
    .bss.stack  : { *(.bss.stack) }
    .bss        : { *(.bss*) *(COMMON) }

    /DISCARD/ : { *(.comment) *(.note*) *(.ARM.attributes) }
}
