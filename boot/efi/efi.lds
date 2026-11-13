/* SPDX-License-Identifier: Apache-2.0 */
/* Link script for the HAVEN EFI tools: one image based at 0, 4 KiB aligned
 * sections, converted to PE/COFF by objcopy (boot/efi/build.sh). The code
 * is position independent and has no relocations, so the firmware may load
 * it at any address; .reloc holds one empty block so the PE file is not
 * marked "relocations stripped". */
OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(efi_main)

SECTIONS
{
	. = 0x1000;
	.text : ALIGN(4096) { *(.text .text.*) }
	.rodata : ALIGN(4096) { *(.rodata .rodata.*) }
	.data : ALIGN(4096) { *(.data .data.*) *(.bss .bss.*) *(COMMON) }
	.reloc : ALIGN(4096) { LONG(0x1000) LONG(12) SHORT(0) SHORT(0) }
	/DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame*) }
}
