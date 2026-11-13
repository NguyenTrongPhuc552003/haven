/* SPDX-License-Identifier: Apache-2.0 */
/* The few UEFI types the HAVEN EFI tools use, declared from the UEFI 2.10
 * specification (4.2 table header, 4.3 system table, 4.4 boot services,
 * 4.6 configuration table, 7.2 memory map, 12.4 simple text output).
 * Only the members up to the last one used are declared: the structures
 * are only read through pointers the firmware gives. AArch64 only. */
#ifndef HAVEN_BOOT_EFI_H
#define HAVEN_BOOT_EFI_H

#include <stdint.h>

typedef uint64_t efi_status;
typedef uint16_t char16;
#define EFI_SUCCESS 0U

struct efi_guid {
	uint32_t d1;
	uint16_t d2;
	uint16_t d3;
	uint8_t d4[8];
};

struct efi_table_header {
	uint64_t signature;
	uint32_t revision;
	uint32_t header_size;
	uint32_t crc32;
	uint32_t reserved;
};

struct efi_text_out {
	void *reset;
	efi_status (*output_string)(struct efi_text_out *self, char16 *s);
};

struct efi_memory_descriptor {
	uint32_t type;
	uint32_t pad;
	uint64_t physical_start;
	uint64_t virtual_start;
	uint64_t number_of_pages;
	uint64_t attribute;
};

struct efi_boot_services {
	struct efi_table_header hdr;
	void *raise_tpl;
	void *restore_tpl;
	void *allocate_pages;
	void *free_pages;
	efi_status (*get_memory_map)(uint64_t *map_size,
				     struct efi_memory_descriptor *map,
				     uint64_t *map_key, uint64_t *desc_size,
				     uint32_t *desc_version);
};

struct efi_configuration_table {
	struct efi_guid guid;
	void *table;
};

struct efi_system_table {
	struct efi_table_header hdr;
	char16 *firmware_vendor;
	uint32_t firmware_revision;
	void *console_in_handle;
	void *con_in;
	void *console_out_handle;
	struct efi_text_out *con_out;
	void *standard_error_handle;
	struct efi_text_out *std_err;
	void *runtime_services;
	struct efi_boot_services *boot_services;
	uint64_t number_of_table_entries;
	struct efi_configuration_table *configuration_table;
};

#endif /* HAVEN_BOOT_EFI_H */
