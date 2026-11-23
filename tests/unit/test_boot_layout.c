/* SPDX-License-Identifier: Apache-2.0 */
/* haven-boot.efi's tables (boot/efi/layout.h), row L3 of
 * docs/methodology/stage2/20261124-loader-cix-p1: every file loads inside
 * one claimed range, no two loads overlap, claims are 2 MiB aligned; on
 * cix-p1 the kernel is at partition A's base and the device tree 8-byte
 * aligned and at most 2 MiB. Built once per platform (tests/CMakeLists.txt). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "efi.h"
#include "layout.h"

static const struct boot_range ranges[] = {BOOT_RANGES};
static const struct boot_range loads[] = {BOOT_LOADS};
#define N(a) (sizeof(a) / sizeof((a)[0]))

static int fails;

static void check(int ok, const char *what, const char *name)
{
	if (!ok) {
		printf("FAIL: %s: %s\n", name, what);
		fails++;
	}
}

static int inside(const struct boot_range *l, const struct boot_range *r)
{
	return l->base >= r->base && l->base + l->size <= r->base + r->size;
}

int main(void)
{
	unsigned int i, j, n;

	for (i = 0; i < N(ranges); i++)
		check(((ranges[i].base | ranges[i].size) & 0x1FFFFFU) == 0U,
		      "claim not 2 MiB aligned", ranges[i].name);
	for (i = 0; i < N(loads); i++) {
		check(loads[i].file[0] != '\0', "load without a file",
		      loads[i].name);
		for (n = 0, j = 0; j < N(ranges); j++)
			n += (unsigned int)inside(&loads[i], &ranges[j]);
		check(n == 1U, "not inside exactly one claimed range",
		      loads[i].name);
		for (j = i + 1; j < N(loads); j++)
			check(loads[i].base + loads[i].size <= loads[j].base ||
				      loads[j].base + loads[j].size <=
					      loads[i].base,
			      "overlaps another load", loads[i].name);
	}
#ifdef HAVEN_PLATFORM_CIX_P1
	check(loads[1].base == PART_A_PA_BASE, "kernel not at A's base",
	      loads[1].name);
	check((loads[2].base & 7U) == 0U && loads[2].size <= 0x200000U,
	      "device tree not 8-byte aligned or over 2 MiB", loads[2].name);
	printf("test_boot_layout: cix-p1, %u claims, %u loads\n",
	       (unsigned int)N(ranges), (unsigned int)N(loads));
#else
	printf("test_boot_layout: qemu-virt, %u claims, %u loads\n",
	       (unsigned int)N(ranges), (unsigned int)N(loads));
#endif
	return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
