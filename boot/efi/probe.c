/* SPDX-License-Identifier: Apache-2.0 */
/* haven-probe.efi: read-only UEFI probe (L0,
 * docs/methodology/stage2/20261109-uefi-loading/README.md).
 *
 * Prints the CPU state an EFI application gets, the memory map and the
 * configuration tables, then returns to the caller. It changes nothing: no
 * ExitBootServices, no allocation, no system-register or device write.
 * Position independent with no data needing relocation, so the firmware
 * may load it anywhere. */
#include "efi.h"

static struct efi_text_out *out;
static char16 wbuf[160];
static uint8_t map_buf[32768] __attribute__((aligned(8)));

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

static void puts_a(const char *s)
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

static void hex(uint64_t v)
{
	char b[19];
	int i;

	b[0] = '0';
	b[1] = 'x';
	for (i = 0; i < 16; i++)
		b[2 + i] = "0123456789abcdef"[(v >> (60 - 4 * i)) & 0xfU];
	b[18] = 0;
	puts_a(b);
}

static void kv(const char *k, uint64_t v)
{
	puts_a("probe: ");
	puts_a(k);
	puts_a(" ");
	hex(v);
	puts_a("\n");
}

#define READ(reg)                                                  \
	({                                                         \
		uint64_t _v;                                       \
		__asm__ volatile("mrs %0, " reg : "=r"(_v));       \
		_v;                                                \
	})

static const char *mem_type(uint32_t t)
{
	switch (t) {
	case 0: return "Reserved";
	case 1: return "LoaderCode";
	case 2: return "LoaderData";
	case 3: return "BootServicesCode";
	case 4: return "BootServicesData";
	case 5: return "RuntimeServicesCode";
	case 6: return "RuntimeServicesData";
	case 7: return "Conventional";
	case 8: return "Unusable";
	case 9: return "ACPIReclaim";
	case 10: return "ACPINVS";
	case 11: return "MMIO";
	case 12: return "MMIOPortSpace";
	case 13: return "PalCode";
	case 14: return "Persistent";
	default: return "Other";
	}
}

static int guid_is(const struct efi_guid *g, uint32_t d1, uint16_t d2,
		   uint16_t d3, uint64_t d4be)
{
	int i;

	if (g->d1 != d1 || g->d2 != d2 || g->d3 != d3)
		return 0;
	for (i = 0; i < 8; i++)
		if (g->d4[i] != (uint8_t)(d4be >> (56 - 8 * i)))
			return 0;
	return 1;
}

static const char *table_name(const struct efi_guid *g)
{
	if (guid_is(g, 0x8868e871, 0xe4f1, 0x11d3, 0xbc220080c73c8881ULL))
		return "ACPI20";
	if (guid_is(g, 0xb1b621d5, 0xf19c, 0x41a5, 0x830bd9152c69aae0ULL))
		return "DTB";
	if (guid_is(g, 0xf2fd1544, 0x9794, 0x4a2c, 0x992ee5bbcf20e394ULL))
		return "SMBIOS3";
	return "other";
}

efi_status efi_main(void *image, struct efi_system_table *st)
{
	struct efi_boot_services *bs = st->boot_services;
	uint64_t el = (READ("CurrentEL") >> 2) & 3U;
	uint64_t size = sizeof(map_buf), key, dsize;
	uint32_t dver;
	uint64_t i;

	(void)image;
	out = st->con_out;
	puts_a("probe: HAVEN-PROBE 1\n");
	kv("CurrentEL", el);
	if (el == 2U) {
		kv("HCR_EL2", READ("hcr_el2"));
		kv("SCTLR_EL2", READ("sctlr_el2"));
		kv("TCR_EL2", READ("tcr_el2"));
		kv("CNTHCTL_EL2", READ("cnthctl_el2"));
	} else {
		puts_a("probe: EL2 registers n/a\n");
	}
	kv("ID_AA64MMFR1_EL1", READ("id_aa64mmfr1_el1"));
	kv("ID_AA64MMFR4_EL1", READ("s3_0_c0_c7_4"));
	kv("ID_AA64PFR0_EL1", READ("id_aa64pfr0_el1"));
	kv("MPIDR_EL1", READ("mpidr_el1"));
	kv("MIDR_EL1", READ("midr_el1"));
	kv("CNTFRQ_EL0", READ("cntfrq_el0"));
	kv("DAIF", READ("daif"));
	kv("probe_at", (uint64_t)(uintptr_t)efi_main);

	if (bs->get_memory_map(&size, (struct efi_memory_descriptor *)map_buf,
			       &key, &dsize, &dver) == EFI_SUCCESS && dsize) {
		for (i = 0; i + dsize <= size; i += dsize) {
			const struct efi_memory_descriptor *d =
				(const void *)(map_buf + i);

			puts_a("probe: mem ");
			hex(d->physical_start);
			puts_a(" pages ");
			hex(d->number_of_pages);
			puts_a(" attr ");
			hex(d->attribute);
			puts_a(" ");
			puts_a(mem_type(d->type));
			puts_a("\n");
		}
	} else {
		puts_a("probe: memory map unavailable\n");
	}

	for (i = 0; i < st->number_of_table_entries; i++) {
		const struct efi_configuration_table *t =
			&st->configuration_table[i];

		puts_a("probe: table ");
		puts_a(table_name(&t->guid));
		puts_a(" ");
		hex(t->guid.d1);
		puts_a(" at ");
		hex((uint64_t)(uintptr_t)t->table);
		puts_a("\n");
	}
	puts_a("probe: end\n");
	return EFI_SUCCESS;
}
