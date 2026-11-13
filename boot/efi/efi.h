/* SPDX-License-Identifier: Apache-2.0 */
/* The few UEFI types the HAVEN EFI tools use, declared from the UEFI 2.10
 * specification (4.2 table header, 4.3 system table, 4.4 boot services,
 * 4.6 configuration table, 7.2 memory map, 9.1 loaded image protocol,
 * 12.4 simple text output, 13.4 simple file system, 13.5 file protocol).
 * Only the members up to the last one used are declared: the structures
 * are only read through pointers the firmware gives. AArch64 only. */
#ifndef HAVEN_BOOT_EFI_H
#define HAVEN_BOOT_EFI_H

#include <stdint.h>

typedef uint64_t efi_status;
typedef uint16_t char16;
typedef void *efi_handle;

#define EFI_SUCCESS 0U
#define EFI_ERROR(n) (0x8000000000000000ULL | (n))
#define EFI_LOAD_ERROR EFI_ERROR(1)
#define EFI_INVALID_PARAMETER EFI_ERROR(2)
#define EFI_UNSUPPORTED EFI_ERROR(3)
#define EFI_BAD_BUFFER_SIZE EFI_ERROR(4)

/* 7.2 AllocatePages: allocate type and memory types */
#define EFI_ALLOCATE_ADDRESS 2U
#define EFI_LOADER_CODE 1U
#define EFI_LOADER_DATA 2U
#define EFI_PAGE_SIZE 4096UL

/* 13.5 file protocol open mode */
#define EFI_FILE_MODE_READ 1ULL

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

/* 4.4: the boot services table in its specified order, up to
 * ExitBootServices (entry 27); entries not used are kept as void *. */
struct efi_boot_services {
	struct efi_table_header hdr;
	void *raise_tpl;
	void *restore_tpl;
	efi_status (*allocate_pages)(uint32_t type, uint32_t memory_type,
				     uint64_t pages, uint64_t *memory);
	efi_status (*free_pages)(uint64_t memory, uint64_t pages);
	efi_status (*get_memory_map)(uint64_t *map_size,
				     struct efi_memory_descriptor *map,
				     uint64_t *map_key, uint64_t *desc_size,
				     uint32_t *desc_version);
	void *allocate_pool;
	void *free_pool;
	void *create_event;
	void *set_timer;
	void *wait_for_event;
	void *signal_event;
	void *close_event;
	void *check_event;
	void *install_protocol_interface;
	void *reinstall_protocol_interface;
	void *uninstall_protocol_interface;
	efi_status (*handle_protocol)(efi_handle handle,
				      const struct efi_guid *protocol,
				      void **interface);
	void *reserved;
	void *register_protocol_notify;
	void *locate_handle;
	void *locate_device_path;
	void *install_configuration_table;
	void *load_image;
	void *start_image;
	void *exit;
	void *unload_image;
	efi_status (*exit_boot_services)(efi_handle image, uint64_t map_key);
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

/* 9.1 EFI_LOADED_IMAGE_PROTOCOL, up to ImageSize */
struct efi_loaded_image {
	uint32_t revision;
	efi_handle parent_handle;
	struct efi_system_table *system_table;
	efi_handle device_handle;
	void *file_path;
	void *reserved;
	uint32_t load_options_size;
	void *load_options;
	void *image_base;
	uint64_t image_size;
};

/* 13.5 EFI_FILE_PROTOCOL, up to Read */
struct efi_file {
	uint64_t revision;
	efi_status (*open)(struct efi_file *self, struct efi_file **new_handle,
			   char16 *name, uint64_t mode, uint64_t attributes);
	efi_status (*close)(struct efi_file *self);
	void *delete_file;
	efi_status (*read)(struct efi_file *self, uint64_t *size, void *buffer);
};

/* 13.4 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL */
struct efi_simple_file_system {
	uint64_t revision;
	efi_status (*open_volume)(struct efi_simple_file_system *self,
				  struct efi_file **root);
};

/* 9.1 and 13.4 protocol GUIDs */
#define EFI_LOADED_IMAGE_GUID \
	{0x5b1b31a1,          \
	 0x9562,              \
	 0x11d2,              \
	 {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
#define EFI_SIMPLE_FILE_SYSTEM_GUID \
	{0x964e5b22,                \
	 0x6459,                    \
	 0x11d2,                    \
	 {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#endif /* HAVEN_BOOT_EFI_H */
