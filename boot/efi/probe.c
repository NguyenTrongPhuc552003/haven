/* SPDX-License-Identifier: Apache-2.0 */
/* haven-probe.efi: read-only UEFI probe (L0,
 * docs/methodology/stage2/20261109-uefi-loading/README.md).
 *
 * Prints the CPU state an EFI application gets, the memory map and the
 * configuration tables, then returns to the caller. It changes nothing: no
 * ExitBootServices, no allocation, no system-register or device write.
 * Position independent with no data needing relocation, so the firmware
 * may load it anywhere. */
#include "efi_lib.h"

static uint8_t map_buf[32768] __attribute__((aligned(8)));

static void kv(const char *k, uint64_t v)
{
	con_kv("probe", k, v);
}

#define READ(reg) READ_SYSREG(reg)

static const char *mem_type(uint32_t t)
{
	switch (t) {
	case 0:
		return "Reserved";
	case 1:
		return "LoaderCode";
	case 2:
		return "LoaderData";
	case 3:
		return "BootServicesCode";
	case 4:
		return "BootServicesData";
	case 5:
		return "RuntimeServicesCode";
	case 6:
		return "RuntimeServicesData";
	case 7:
		return "Conventional";
	case 8:
		return "Unusable";
	case 9:
		return "ACPIReclaim";
	case 10:
		return "ACPINVS";
	case 11:
		return "MMIO";
	case 12:
		return "MMIOPortSpace";
	case 13:
		return "PalCode";
	case 14:
		return "Persistent";
	default:
		return "Other";
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
	con_init(st->con_out);
	con_puts("probe: HAVEN-PROBE 1\n");
	kv("CurrentEL", el);
	if (el == 2U) {
		kv("HCR_EL2", READ("hcr_el2"));
		kv("SCTLR_EL2", READ("sctlr_el2"));
		kv("TCR_EL2", READ("tcr_el2"));
		kv("CNTHCTL_EL2", READ("cnthctl_el2"));
	} else {
		con_puts("probe: EL2 registers n/a\n");
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
			       &key, &dsize, &dver) == EFI_SUCCESS &&
	    dsize) {
		for (i = 0; i + dsize <= size; i += dsize) {
			const struct efi_memory_descriptor *d =
				(const void *)(map_buf + i);

			con_puts("probe: mem ");
			con_hex(d->physical_start);
			con_puts(" pages ");
			con_hex(d->number_of_pages);
			con_puts(" attr ");
			con_hex(d->attribute);
			con_puts(" ");
			con_puts(mem_type(d->type));
			con_puts("\n");
		}
	} else {
		con_puts("probe: memory map unavailable\n");
	}

	for (i = 0; i < st->number_of_table_entries; i++) {
		const struct efi_configuration_table *t =
			&st->configuration_table[i];

		con_puts("probe: table ");
		con_puts(table_name(&t->guid));
		con_puts(" ");
		con_hex(t->guid.d1);
		con_puts(" at ");
		con_hex((uint64_t)(uintptr_t)t->table);
		con_puts("\n");
	}
	con_puts("probe: end\n");
	return EFI_SUCCESS;
}
