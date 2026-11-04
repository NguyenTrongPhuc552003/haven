/* SPDX-License-Identifier: Apache-2.0 */
/* Stage-2 page table descriptor formats for ARMv8-A / ARMv9-A.
 * Covers both 4KB granule (3-level walk) and 64KB granule.
 * All Haven partitions use 4KB granule with 39-bit IPA (T0SZ=25). */

#ifndef HAVEN_ASM_PAGE_H
#define HAVEN_ASM_PAGE_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Page / block sizes
 * ----------------------------------------------------------------------- */

#define HV_PAGE_SHIFT 12
#define HV_PAGE_SIZE (UINT64_C(1) << HV_PAGE_SHIFT) /* 4 KiB */
#define HV_PAGE_MASK (~(HV_PAGE_SIZE - 1))

#define HV_BLOCK_SHIFT_L2 21
#define HV_BLOCK_SIZE_L2 (UINT64_C(1) << HV_BLOCK_SHIFT_L2) /* 2 MiB */
#define HV_BLOCK_MASK_L2 (~(HV_BLOCK_SIZE_L2 - 1))

#define HV_BLOCK_SHIFT_L1 30
#define HV_BLOCK_SIZE_L1 (UINT64_C(1) << HV_BLOCK_SHIFT_L1) /* 1 GiB */
#define HV_BLOCK_MASK_L1 (~(HV_BLOCK_SIZE_L1 - 1))

/* -----------------------------------------------------------------------
 * Stage-2 descriptor types (bits [1:0])
 * ----------------------------------------------------------------------- */

#define S2_DESC_INVALID UINT64_C(0x0) /* Descriptor is invalid */
#define S2_DESC_BLOCK UINT64_C(0x1) /* Level 1/2 block descriptor */
#define S2_DESC_TABLE UINT64_C(0x3) /* Level 0/1/2 table descriptor */
#define S2_DESC_PAGE UINT64_C(0x3) /* Level 3 page descriptor */

#define s2_desc_is_valid(d) (((d) & 0x1) != 0)
#define s2_desc_is_table(d) (((d) & 0x3) == S2_DESC_TABLE)
#define s2_desc_is_block(d) (((d) & 0x3) == S2_DESC_BLOCK)
#define s2_desc_is_page(d) (((d) & 0x3) == S2_DESC_PAGE)
#define s2_desc_pa(d) ((d) & UINT64_C(0x0000fffffffff000)) /* Output address */

/* -----------------------------------------------------------------------
 * Stage-2 block/page lower attributes (bits [11:2])
 * ----------------------------------------------------------------------- */

/* MemAttr[3:0] - stage-2 memory attributes (MAIR index style) */
#define S2_MEMATTR_SHIFT 2
#define S2_MEMATTR_MASK (UINT64_C(0xf) << S2_MEMATTR_SHIFT)

/* Stage-2 memory type encodings (ARM DDI 0487, D8.5.5) */
#define S2_MEMATTR_DEVICE_nGnRnE \
	(UINT64_C(0x0) << S2_MEMATTR_SHIFT) /* Device nGnRnE */
#define S2_MEMATTR_DEVICE_nGnRE \
	(UINT64_C(0x1) << S2_MEMATTR_SHIFT) /* Device nGnRE */
#define S2_MEMATTR_DEVICE_nGRE \
	(UINT64_C(0x2) << S2_MEMATTR_SHIFT) /* Device nGRE */
#define S2_MEMATTR_DEVICE_GRE \
	(UINT64_C(0x3) << S2_MEMATTR_SHIFT) /* Device GRE */
#define S2_MEMATTR_NORMAL_WT (UINT64_C(0x9) << S2_MEMATTR_SHIFT) /* Normal WT */
#define S2_MEMATTR_NORMAL_WB \
	(UINT64_C(0xf) << S2_MEMATTR_SHIFT) /* Normal WB WA RA */
#define S2_MEMATTR_NORMAL_NC (UINT64_C(0x5) << S2_MEMATTR_SHIFT) /* Normal NC */

/* S2AP[1:0] - stage-2 access permissions (bits [7:6]) */
#define S2AP_SHIFT 6
#define S2AP_NONE (UINT64_C(0x0) << S2AP_SHIFT) /* No access */
#define S2AP_RO (UINT64_C(0x1) << S2AP_SHIFT) /* Read-only */
#define S2AP_WO (UINT64_C(0x2) << S2AP_SHIFT) /* Write-only */
#define S2AP_RW (UINT64_C(0x3) << S2AP_SHIFT) /* Read-write */

/* SH[1:0] - shareability (bits [9:8]) */
#define S2_SH_SHIFT 8
#define S2_SH_NS (UINT64_C(0x0) << S2_SH_SHIFT) /* Non-shareable */
#define S2_SH_OS (UINT64_C(0x2) << S2_SH_SHIFT) /* Outer Shareable */
#define S2_SH_IS (UINT64_C(0x3) << S2_SH_SHIFT) /* Inner Shareable */

/* AF - Access Flag (bit 10): must be 1 to avoid Access Flag Fault */
#define S2_AF (UINT64_C(1) << 10)

/* -----------------------------------------------------------------------
 * Stage-2 block/page upper attributes (bits [63:51] and [54:52])
 * ----------------------------------------------------------------------- */

/* XN[1:0] - stage-2 execute-never field, bits [54:53] (MM2).
 * Without FEAT_XNX only bit 54 is defined (1 = execute-never) and bit 53 is
 * ignored. With FEAT_XNX the field encodes 00: EL1 and EL0 may execute,
 * 01: EL0 only, 10: neither, 11: EL1 only. 0b10 is the one value that means
 * "never execute" on both kinds of CPU. The previous S2_XN_ALL (0b11) left
 * mappings executable at EL1 on CPUs with FEAT_XNX. Source for the encoding:
 * QEMU's ARMv8.2-TTS2UXN implementation; confirm against the Arm ARM. */
#define S2_XN_SHIFT 53
#define S2_XN_MASK (UINT64_C(3) << S2_XN_SHIFT)
#define S2_XN_EXEC \
	(UINT64_C(0) << S2_XN_SHIFT) /* 00: executable at EL1 and EL0 */
#define S2_XN_NEVER (UINT64_C(2) << S2_XN_SHIFT) /* 10: never executable */
#define S2_XN_ALL S2_XN_NEVER /* kept for existing users; now 0b10 */

/* Contiguous hint (bit 52) */
#define S2_CONT (UINT64_C(1) << 52)

/* -----------------------------------------------------------------------
 * Composite descriptor builders
 * ----------------------------------------------------------------------- */

/* Normal memory page: RW, Inner Shareable, WB, executable */
#define S2_PAGE_NORMAL_RW(pa)                                            \
	((pa) | S2_DESC_PAGE | S2_MEMATTR_NORMAL_WB | S2_SH_IS | S2_AF | \
	 S2AP_RW)

/* Normal memory page: RO, Inner Shareable, WB, non-executable */
#define S2_PAGE_NORMAL_RO(pa)                                            \
	((pa) | S2_DESC_PAGE | S2_MEMATTR_NORMAL_WB | S2_SH_IS | S2_AF | \
	 S2AP_RO | S2_XN_ALL)

/* Device memory page: RW, non-shareable, device, non-executable */
#define S2_PAGE_DEVICE_RW(pa)                                               \
	((pa) | S2_DESC_PAGE | S2_MEMATTR_DEVICE_nGnRE | S2_SH_NS | S2_AF | \
	 S2AP_RW | S2_XN_ALL)

/* 2MiB block: Normal RW */
#define S2_BLOCK2M_NORMAL_RW(pa)                                          \
	((pa) | S2_DESC_BLOCK | S2_MEMATTR_NORMAL_WB | S2_SH_IS | S2_AF | \
	 S2AP_RW)

/* Table descriptor: just PA of next-level table */
#define S2_TABLE(pa) ((pa) | S2_DESC_TABLE)

/* -----------------------------------------------------------------------
 * Page table index extraction from IPA
 * IPA layout for 4K/3-level (T0SZ=25, 39-bit IPA):
 *   [38:30] → L1 index (9 bits)
 *   [29:21] → L2 index (9 bits)
 *   [20:12] → L3 index (9 bits)
 *   [11:0]  → page offset
 * ----------------------------------------------------------------------- */

#define S2_PGD_SHIFT 30
#define S2_PMD_SHIFT 21
#define S2_PTE_SHIFT 12
#define S2_PTRS_PER_LEVEL 512 /* 9-bit index → 512 entries per table */
#define S2_TABLE_SIZE (S2_PTRS_PER_LEVEL * sizeof(uint64_t)) /* 4 KiB */

#define s2_l1_index(ipa) (((ipa) >> S2_PGD_SHIFT) & 0x1ff)
#define s2_l2_index(ipa) (((ipa) >> S2_PMD_SHIFT) & 0x1ff)
#define s2_l3_index(ipa) (((ipa) >> S2_PTE_SHIFT) & 0x1ff)

/* Maximum IPA: 2^39 = 512 GiB (enough for any current SoC) */
#define S2_IPA_MAX_BITS 39
#define S2_IPA_SIZE (UINT64_C(1) << S2_IPA_MAX_BITS)

/* -----------------------------------------------------------------------
 * Physical address space sizes (VTCR_EL2.PS)
 * ----------------------------------------------------------------------- */

#define PS_32BIT 0 /* 4 GiB */
#define PS_36BIT 1 /* 64 GiB */
#define PS_40BIT 2 /* 1 TiB - sufficient for all current ARM SoCs */
#define PS_42BIT 3 /* 4 TiB */
#define PS_44BIT 4 /* 16 TiB */
#define PS_48BIT 5 /* 256 TiB */
#define PS_52BIT 6 /* 4 PiB (LPA) */

#endif /* HAVEN_ASM_PAGE_H */
