/* SPDX-License-Identifier: Apache-2.0 */
/* What haven-boot.efi claims and loads, taken from HAVEN's own platform
 * header so the loader and HAVEN cannot disagree on an address: qemu-virt
 * (L1b, docs/methodology/stage2/20261109-uefi-loading/) and cix-p1
 * (docs/methodology/stage2/20261124-loader-cix-p1/). build.sh puts the
 * platform's directory on the include path. */
#ifndef HAVEN_BOOT_LAYOUT_H
#define HAVEN_BOOT_LAYOUT_H

#include "memory.h" /* src/platform/<platform>/memory.h, by -I in build.sh */

/* Names are arrays, not pointers: a pointer in static data would need a
 * relocation, and the EFI tools are built without any. */
struct boot_range {
	char name[12];
	uint64_t base;
	uint64_t size;
	uint32_t type; /* EFI_LOADER_CODE or EFI_LOADER_DATA */
	char file[16]; /* image loaded at base, or "" */
};

/* BOOT_RANGES are claimed (and zeroed) at their fixed addresses; BOOT_LOADS
 * are files loaded at base, at most size bytes, inside a claimed range,
 * after every claim. */
#ifdef HAVEN_PLATFORM_CIX_P1
#define BOOT_RANGES                                                           \
	{"haven", HAVEN_LOAD_PA, HAVEN_SIZE, EFI_LOADER_CODE, ""},            \
		{"part-a", PART_A_PA_BASE, PART_A_SIZE, EFI_LOADER_DATA, ""}, \
		{"part-b", PART_B_PA_BASE, PART_B_SIZE, EFI_LOADER_DATA, ""}
#define BOOT_LOADS                                                          \
	{"haven", HAVEN_LOAD_PA, HAVEN_SIZE, EFI_LOADER_CODE, "haven.bin"}, \
		{"a-kernel", PART_A_PA_BASE, CIX_PART_A_KERNEL_MAX,         \
		 EFI_LOADER_DATA, "Image"},                                 \
		{"a-dtb", CIX_PART_A_DTB_IPA, CIX_PART_A_DTB_MAX,           \
		 EFI_LOADER_DATA, "partition-a.dtb"},                       \
		{"a-initrd", CIX_PART_A_INITRD_IPA, CIX_PART_A_INITRD_MAX,  \
		 EFI_LOADER_DATA, "initramfs.cpio"},                        \
		{"part-b", PART_B_PA_BASE, PART_B_SIZE, EFI_LOADER_DATA,    \
		 "guest_b.bin"}
#define BOOT_UART CIX_UART3_BASE
#else /* qemu-virt */
#define BOOT_RANGES                                                           \
	{"haven", HAVEN_LOAD_PA, HAVEN_SIZE, EFI_LOADER_CODE, ""},            \
		{"part-a", PART_A_PA_BASE, PART_A_SIZE, EFI_LOADER_DATA, ""}, \
		{"part-b", PART_B_PA_BASE, PART_B_SIZE, EFI_LOADER_DATA, ""}, \
		{"shared", SHARED_PA_BASE, SHARED_SIZE, EFI_LOADER_DATA, ""}, \
		{"part-a-ro", PART_A_RO_PA_BASE, PART_A_RO_SIZE,              \
		 EFI_LOADER_DATA, ""}
#define BOOT_LOADS                                                          \
	{"haven", HAVEN_LOAD_PA, HAVEN_SIZE, EFI_LOADER_CODE, "haven.bin"}, \
		{"part-a", PART_A_PA_BASE, PART_A_SIZE, EFI_LOADER_DATA,    \
		 "guest_a.bin"},                                            \
		{"part-b", PART_B_PA_BASE, PART_B_SIZE, EFI_LOADER_DATA,    \
		 "guest_b.bin"}
#define BOOT_UART QEMU_UART_BASE
#endif

#define BOOT_ENTRY HAVEN_LOAD_PA

#endif /* HAVEN_BOOT_LAYOUT_H */
