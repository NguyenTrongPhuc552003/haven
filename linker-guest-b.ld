/*
 * linker-guest-b.ld - Guest B (RTOS-class) stub linker script.
 *
 * Links the guest_b_entry.S stub at VMA 0x60000000 (Partition B IPA base).
 * Haven loads the resulting binary at PA 0xA0800000 (PART_B_PA_BASE) and
 * stage-2 maps PA 0xA0800000 → IPA 0x60000000 for Partition B.
 *
 * Guest B is launched on CPU 2 via hv_arch_start_partition() in
 * src/core/partition.c (added in the secondary-CPU bring-up phase).
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

ENTRY(_guest_b_start)

SECTIONS
{
    . = 0x60000000;

    .text.entry : { *(.text.entry) }
    .text       : { *(.text*) }
    .rodata     : { *(.rodata*) }
    .data       : { *(.data*) }

    . = ALIGN(4096);
    .bss.stack  : { *(.bss.stack) }
    .bss        : { *(.bss*) *(COMMON) }

    /DISCARD/ : { *(.comment) *(.note*) *(.ARM.attributes) }
}
