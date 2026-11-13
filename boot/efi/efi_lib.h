/* SPDX-License-Identifier: Apache-2.0 */
/* Helpers shared by the HAVEN EFI tools (probe and loader): console output
 * through the firmware's text protocol, hex printing, the two memory
 * functions the compiler may call, and system-register reads. */
#ifndef HAVEN_BOOT_EFI_LIB_H
#define HAVEN_BOOT_EFI_LIB_H

#include "efi.h"

/* Console output through st->con_out; set once in efi_main. */
void con_init(struct efi_text_out *out);
void con_puts(const char *s);
void con_hex(uint64_t v);
/* "<tag>: <key> 0x...\n" */
void con_kv(const char *tag, const char *key, uint64_t v);

void *memset(void *d, int c, unsigned long n);
void *memcpy(void *d, const void *s, unsigned long n);

#define READ_SYSREG(reg)                                     \
	({                                                   \
		uint64_t _v;                                 \
		__asm__ volatile("mrs %0, " reg : "=r"(_v)); \
		_v;                                          \
	})

#endif /* HAVEN_BOOT_EFI_LIB_H */
