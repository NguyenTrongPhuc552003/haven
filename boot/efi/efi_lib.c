/* SPDX-License-Identifier: Apache-2.0 */
/* Helpers shared by the HAVEN EFI tools; see efi_lib.h. */
#include "efi_lib.h"

static struct efi_text_out *out;
static char16 wbuf[160];

void con_init(struct efi_text_out *o)
{
	out = o;
}

void *memset(void *d, int c, unsigned long n)
{
	uint8_t *p = d;

	while (n--)
		*p++ = (uint8_t)c;
	return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
	uint8_t *p = d;
	const uint8_t *q = s;

	while (n--)
		*p++ = *q++;
	return d;
}

void con_puts(const char *s)
{
	unsigned int n = 0;

	while (*s) {
		if (*s == '\n')
			wbuf[n++] = '\r';
		wbuf[n++] = (char16)*s++;
		if (n >= sizeof(wbuf) / sizeof(wbuf[0]) - 3U || !*s) {
			wbuf[n] = 0;
			out->output_string(out, wbuf);
			n = 0;
		}
	}
}

void con_hex(uint64_t v)
{
	char b[19];
	int i;

	b[0] = '0';
	b[1] = 'x';
	for (i = 0; i < 16; i++)
		b[2 + i] = "0123456789abcdef"[(v >> (60 - 4 * i)) & 0xfU];
	b[18] = 0;
	con_puts(b);
}

void con_kv(const char *tag, const char *key, uint64_t v)
{
	con_puts(tag);
	con_puts(": ");
	con_puts(key);
	con_puts(" ");
	con_hex(v);
	con_puts("\n");
}
