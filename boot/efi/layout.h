/* SPDX-License-Identifier: Apache-2.0 */
/* What haven-boot.efi claims and loads, taken from HAVEN's own platform
 * header so the loader and HAVEN cannot disagree on an address. Only
 * qemu-virt today (L1b, docs/methodology/stage2/20261109-uefi-loading/). */
#ifndef HAVEN_BOOT_LAYOUT_H
#define HAVEN_BOOT_LAYOUT_H

#include "memory.h" /* src/platform/qemu-virt/memory.h, by -I in build.sh */

/* Names are arrays, not pointers: a pointer in static data would need a
 * relocation, and the EFI tools are built without any. */
struct boot_range {
	char name[12];
	uint64_t base;
	uint64_t size;
	uint32_t type; /* EFI_LOADER_CODE or EFI_LOADER_DATA */
	char file[16]; /* image loaded at base, or "" */
};

#define BOOT_RANGES                                                           \
	{"haven", HAVEN_LOAD_PA, HAVEN_SIZE, EFI_LOADER_CODE, "haven.bin"},   \
		{"part-a", PART_A_PA_BASE, PART_A_SIZE, EFI_LOADER_DATA,      \
		 "guest_a.bin"},                                              \
		{"part-b", PART_B_PA_BASE, PART_B_SIZE, EFI_LOADER_DATA,      \
		 "guest_b.bin"},                                              \
		{"shared", SHARED_PA_BASE, SHARED_SIZE, EFI_LOADER_DATA, ""}, \
		{"part-a-ro", PART_A_RO_PA_BASE, PART_A_RO_SIZE,              \
		 EFI_LOADER_DATA, ""}

#define BOOT_ENTRY HAVEN_LOAD_PA
#define BOOT_UART QEMU_UART_BASE

#endif /* HAVEN_BOOT_LAYOUT_H */
