/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/include/asm/el2_mmu.h - Haven's own EL2 stage-1 translation
 * (FINDINGS A1, owner decision 2026-11-02, option A).
 *
 * Identity map, 4 KiB granule, 39-bit VA starting at level 1, 40-bit PA.
 * Haven's image (code, data, its stage-2 and SMMU structures) is Normal
 * write-back inner shareable, so Haven, the CPU stage-2 walker (VTCR_EL2
 * WB, IS) and the SMMU (SMMU_CR1 WB, ISH) all see the same coherent,
 * cacheable view of those structures. Device registers are Device-nGnRE
 * and never executable. Partition memory is not mapped at EL2.
 *
 * Descriptor and register layouts: Arm ARM D8 (VMSAv8-64), for the
 * single-range EL2 regime (HCR_EL2.E2H = 0).
 */
#ifndef HAVEN_ASM_EL2_MMU_H
#define HAVEN_ASM_EL2_MMU_H

/* MAIR_EL2: Attr0 = Device-nGnRE (0x04), Attr1 = Normal WB RA WA (0xff). */
#define EL2_MAIR_VALUE 0xff04
#define EL2_ATTR_DEVICE 0
#define EL2_ATTR_NORMAL 1

/* TCR_EL2: T0SZ = 25 (39-bit VA), IRGN0 = ORGN0 = 1 (WB RA WA),
 * SH0 = 3 (inner shareable), TG0 = 0 (4 KiB), PS = 2 (40-bit PA),
 * RES1 bits 31 and 23. */
#define EL2_TCR_VALUE 0x80823519

#define EL2_SCTLR_M (1 << 0)
#define EL2_SCTLR_C (1 << 2)
#define EL2_SCTLR_I (1 << 12)

#ifndef __ASSEMBLY__
#include <stdint.h>

#define HV_EL2_DEVICE 0U
#define HV_EL2_NORMAL 1U

/* One EL2 mapping: pa and size aligned to 2 MiB (blocks of 2 MiB or
 * 1 GiB). Each platform defines its list; an empty list keeps EL2
 * translation off (legacy platforms). */
struct hv_el2_region {
	uint64_t pa;
	uint64_t size;
	uint32_t type; /* HV_EL2_DEVICE or HV_EL2_NORMAL */
};
extern const struct hv_el2_region hv_el2_regions[];
extern const uint32_t hv_el2_region_count;

/* TTBR0_EL2 value, 0 while EL2 translation is off.  Written by the boot
 * CPU before it enables translation; read by secondary_entry. */
extern volatile uint64_t hv_el2_ttbr;

/* Boot CPU: build the tables from hv_el2_regions.  Returns 0, or a
 * negative error (bad alignment, too many tables, image not covered). */
int hv_arch_el2_mmu_build(void);
/* Boot CPU: discard stale cache lines over Haven's image, then turn EL2
 * translation and caches on.  Secondaries do the same in boot.S without
 * the invalidation (it would discard the boot CPU's dirty lines). */
void hv_arch_el2_mmu_enable(void);
#endif

#endif
