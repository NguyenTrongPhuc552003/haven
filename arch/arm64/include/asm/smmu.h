/* SPDX-License-Identifier: Apache-2.0 */
/* ARM SMMUv3 MMIO register offsets and stream table entry definitions.
 * Based on ARM IHI 0070E (SMMU Architecture Specification v3.3). */

#ifndef HAVEN_ASM_SMMU_H
#define HAVEN_ASM_SMMU_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * SMMUv3 MMIO page layout
 * Page 0 (offset 0x0000): main control, IDR, stream table, command queue
 * Page 1 (offset 0x10000): fault information, event queue
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Identification registers (read-only)
 * ----------------------------------------------------------------------- */

#define SMMU_IDR0 0x0000 /* Identification Register 0 */
#define SMMU_IDR1 0x0004 /* Identification Register 1 */
#define SMMU_IDR2 0x0008 /* Identification Register 2 */
#define SMMU_IDR3 0x000c /* Identification Register 3 */
#define SMMU_IDR4 0x0010 /* Identification Register 4 */
#define SMMU_IDR5 0x0014 /* Identification Register 5 */
#define SMMU_IIDR 0x0018 /* Implementation Identification Register */
#define SMMU_AIDR 0x001c /* Architecture Identification Register */

/* SMMU_IDR0 bits */
#define IDR0_S1P (1U << 1) /* Stage-1 translation present */
#define IDR0_S2P (1U << 0) /* Stage-2 translation present */
#define IDR0_S1TS (1U << 2) /* Stage-1 translation subpages */
#define IDR0_TT4K (1U << 6) /* 4KB translation granule */
#define IDR0_TT64K (1U << 7) /* 64KB translation granule */
#define IDR0_TT16K (1U << 8) /* 16KB translation granule */
#define IDR0_BTM (1U << 11) /* Broadcast TLB maintenance */
#define IDR0_HTTU_SHIFT 6
#define IDR0_HTTU_MASK (3U << IDR0_HTTU_SHIFT)
#define IDR0_HTTU_AF (1U << 6) /* Access flag updates */
#define IDR0_HTTU_AFDB (2U << 6) /* AF and dirty bit updates */
#define IDR0_COHACC (1U << 4) /* Coherent access */
#define IDR0_ASID16 (1U << 12) /* 16-bit ASID support */
#define IDR0_ATS (1U << 10) /* ATS supported */
#define IDR0_PRI (1U << 16) /* Page request interface */
#define IDR0_VMID16 (1U << 18) /* 16-bit VMID */
#define IDR0_CD2L (1U << 19) /* 2-level context descriptor */
#define IDR0_VATOS (1U << 20) /* VA Translation Object Service */
#define IDR0_MSI (1U << 13) /* MSI */
#define IDR0_SEV (1U << 14) /* Send Event */
#define IDR0_ATOS (1U << 15) /* Address Translation Operations */
#define IDR0_HYP (1U << 9) /* Hypervisor mode */
#define IDR0_DORMHINT (1U << 24) /* Dormancy hint */
#define IDR0_RIL (1U << 31) /* Range invalidation */
#define IDR0_ST_LVL_SHIFT 27
#define IDR0_ST_LVL_MASK (3U << IDR0_ST_LVL_SHIFT)
#define IDR0_ST_LVL_LINEAR (0U << IDR0_ST_LVL_SHIFT) /* Linear stream table */
#define IDR0_ST_LVL_2 (1U << IDR0_ST_LVL_SHIFT) /* 2-level stream table */

/* SMMU_IDR1 bits */
#define IDR1_SIDSIZE_SHIFT 0
#define IDR1_SIDSIZE_MASK (0x3f) /* Bits in StreamID */
#define IDR1_SSIDSIZE_SHIFT 6
#define IDR1_SSIDSIZE_MASK (0x1f << 6)
#define IDR1_TABLES_PRESET (1U << 30) /* Stream tables in secure memory */

/* -----------------------------------------------------------------------
 * Global control and status registers
 * ----------------------------------------------------------------------- */

#define SMMU_CR0 0x0020 /* Global Control Register 0 */
#define SMMU_CR0ACK 0x0024 /* Global Control Acknowledge Register */
#define SMMU_CR1 0x0028 /* Global Control Register 1 */
#define SMMU_CR2 0x002c /* Global Control Register 2 */
#define SMMU_STATUSR 0x0040 /* Global Status Register */
#define SMMU_GBPA 0x0044 /* Global Bypass Attribute */
#define SMMU_AGBPA 0x0048 /* Alternate Global Bypass Attribute */
#define SMMU_IRQ_CTRL 0x0050 /* IRQ Control Register */
#define SMMU_IRQ_CTRLACK 0x0054 /* IRQ Control Acknowledge Register */
#define SMMU_GERROR 0x0060 /* Global Error Register */
#define SMMU_GERRORN 0x0064 /* Global Error Clear Register */
#define SMMU_GERROR_IRQ_CFG0 0x0068 /* Global Error IRQ Config 0 */
#define SMMU_GERROR_IRQ_CFG1 0x006c /* Global Error IRQ Config 1 */
#define SMMU_GERROR_IRQ_CFG2 0x0070 /* Global Error IRQ Config 2 */

/* SMMU_CR0 bits */
#define CR0_SMMU_ENABLE (1U << 0) /* Enable the SMMU */
#define CR0_PRIQ_ENABLE (1U << 1) /* Enable Page Request Interface Queue */
#define CR0_EVTQ_ENABLE (1U << 2) /* Enable Event Queue */
#define CR0_CMDQ_ENABLE (1U << 3) /* Enable Command Queue */
#define CR0_ATS_CHECK (1U << 4) /* Enable ATS check */
#define CR0_VMW (1U << 6) /* Virtual Machine Watchdog */
#define CR0_TRANSLATIONEN (CR0_SMMU_ENABLE)

/* SMMU_CR1 bits */
#define CR1_SIF (1U << 0) /* Secure Instruction Fetch */
#define CR1_CMDQ_SH_SHIFT 4
#define CR1_CMDQ_SH_IS (3U << 4) /* Inner Shareable Command Queue */
#define CR1_CMDQ_OC_WB (1U << 6) /* Write-back cacheable Command Queue */
#define CR1_CMDQ_IC_WB (1U << 8) /* Instruction cacheable Command Queue */
#define CR1_EVTQ_SH_IS (3U << 10) /* Inner Shareable Event Queue */
#define CR1_EVTQ_OC_WB (1U << 12)
#define CR1_EVTQ_IC_WB (1U << 14)
#define CR1_STAB_SH_IS (3U << 16) /* Inner Shareable Stream Table */
#define CR1_STAB_OC_WB (1U << 18)
#define CR1_STAB_IC_WB (1U << 20)

/* -----------------------------------------------------------------------
 * Stream table base address register
 * ----------------------------------------------------------------------- */

#define SMMU_STRTAB_BASE 0x0080 /* Stream Table Base Address */
#define SMMU_STRTAB_BASE_CFG 0x0088 /* Stream Table Base Config */

/* SMMU_STRTAB_BASE fields */
#define STRTAB_BASE_RA (1UL << 62) /* Read-Allocate hint */
#define STRTAB_BASE_ADDR_MASK (0x3fffffffffffcUL) /* Bits [49:6] */

/* SMMU_STRTAB_BASE_CFG fields */
#define STRTAB_BASE_CFG_FMT_SHIFT 16
#define STRTAB_BASE_CFG_FMT_LINEAR (0U << 16) /* Linear stream table */
#define STRTAB_BASE_CFG_FMT_2LVL (1U << 16) /* 2-level stream table */
#define STRTAB_BASE_CFG_SPLIT_SHIFT 6
#define STRTAB_BASE_CFG_SPLIT(x) ((x) << 6) /* 2-level split bits (8 common) */
#define STRTAB_BASE_CFG_LOG2SIZE_SHIFT 0
#define STRTAB_BASE_CFG_LOG2SIZE(x) \
	((x) & 0x3f) /* log2 of linear table entries */

/* -----------------------------------------------------------------------
 * Command Queue
 * ----------------------------------------------------------------------- */

#define SMMU_CMDQ_BASE 0x0090
#define SMMU_CMDQ_PROD 0x008c /* Command Queue Producer */
#define SMMU_CMDQ_CONS 0x0090 /* Command Queue Consumer */

/* Command opcodes */
#define CMDQ_OP_PREFETCH_CONFIG 0x01
#define CMDQ_OP_PREFETCH_ADDR 0x02
#define CMDQ_OP_CFGI_STE 0x03 /* Invalidate STE cache for a SID */
#define CMDQ_OP_CFGI_STE_RANGE 0x04
#define CMDQ_OP_CFGI_CD 0x05 /* Invalidate CD cache */
#define CMDQ_OP_CFGI_CD_ALL 0x06
#define CMDQ_OP_CFGI_ALL 0x04 /* Invalidate all config caches */
#define CMDQ_OP_TLBI_NH_ALL 0x10 /* NH TLB invalidate all */
#define CMDQ_OP_TLBI_NH_ASID 0x11 /* NH TLB invalidate ASID */
#define CMDQ_OP_TLBI_NH_VA 0x12 /* NH TLB invalidate VA */
#define CMDQ_OP_TLBI_EL2_ALL 0x20 /* EL2 TLB invalidate all */
#define CMDQ_OP_TLBI_S12_VMALL 0x28 /* Stage-2 TLB invalidate all (VMID) */
#define CMDQ_OP_TLBI_S2_IPA 0x2a /* Stage-2 TLB invalidate IPA */
#define CMDQ_OP_TLBI_NSNH_ALL 0x30 /* Non-secure NH TLB invalidate all */
#define CMDQ_OP_ATC_INV 0x40 /* ATC invalidate */
#define CMDQ_OP_PRI_RESP 0x41 /* Page Request Response */
#define CMDQ_OP_RESUME 0x44
#define CMDQ_OP_STALL_TERM 0x45
#define CMDQ_OP_SYNC 0x46 /* Sync: wait for all prior commands */

/* -----------------------------------------------------------------------
 * Stream Table Entry (STE) format
 * Each STE is 64 bytes (8 × uint64_t).
 * ----------------------------------------------------------------------- */

#define STE_SIZE_BYTES 64
#define STE_DWORDS 8 /* 8 × 64-bit words */

/* Word 0 */
#define STE_0_VALID (1UL << 0) /* STE is valid */
#define STE_0_CONFIG_SHIFT 1
#define STE_0_CONFIG_MASK (7UL << 1)
#define STE_0_CONFIG_ABORT (0UL << 1) /* Abort all transactions */
#define STE_0_CONFIG_BYPASS (4UL << 1) /* Bypass SMMU (pass-through) */
#define STE_0_CONFIG_S1_TRANS (5UL << 1) /* Stage-1 translation only */
#define STE_0_CONFIG_S2_TRANS (6UL << 1) /* Stage-2 translation only */
#define STE_0_CONFIG_S12_TRANS (7UL << 1) /* Both stage-1 and stage-2 */
#define STE_0_S1FMT_LINEAR (0UL << 4) /* Linear CD table */
#define STE_0_S1FMT_2LVL (1UL << 4) /* 2-level CD table */
#define STE_0_S1CDMAX_SHIFT 6
#define STE_0_SHCFG_INC (1UL << 44) /* Incoming shareability attribute */
#define STE_0_SHCFG_OVR (2UL << 44) /* Override with NSH/ISH */
#define STE_0_SHCFG_NSH (3UL << 44) /* Force non-shareable */
#define STE_0_MTCFG (1UL << 46) /* Memory type config override */
#define STE_0_ALLOCCFG_SHIFT 48
#define STE_0_NSCFG_SHIFT 52
#define STE_0_PRIVCFG_SHIFT 54
#define STE_0_INSTCFG_SHIFT 56

/* Word 2 - Stage-2 config */
#define STE_2_S2VMID_SHIFT 0
#define STE_2_S2VMID_MASK (0xffff) /* VMID for stage-2 */
#define STE_2_S2T0SZ_SHIFT 32
#define STE_2_S2T0SZ_MASK (0x3fUL << 32)
#define STE_2_S2SL0_SHIFT 38
#define STE_2_S2SL0_MASK (3UL << 38)
#define STE_2_S2IR0_SHIFT 40
#define STE_2_S2OR0_SHIFT 42
#define STE_2_S2SH0_SHIFT 44
#define STE_2_S2SH0_IS (3UL << 44) /* Inner Shareable */
#define STE_2_S2TG_4K (0UL << 46) /* 4KB granule */
#define STE_2_S2TG_64K (1UL << 46) /* 64KB granule */
#define STE_2_S2TG_16K (2UL << 46) /* 16KB granule */
#define STE_2_S2PS_SHIFT 48
#define STE_2_S2PS_40BIT (2UL << 48) /* 40-bit PA */
#define STE_2_S2AA64 (1UL << 51) /* AArch64 stage-2 tables */
#define STE_2_S2ENDI (1UL << 52) /* Endianness for stage-2 */
#define STE_2_S2AFFD (1UL << 53) /* AF fault disabled */
#define STE_2_S2PTW (1UL << 54) /* Protected Table Walk */
#define STE_2_S2HD (1UL << 55) /* Hardware Dirty flag */
#define STE_2_S2HA (1UL << 56) /* Hardware Access flag */
#define STE_2_S2S (1UL << 57) /* Secure stage-2 */
#define STE_2_S2R (1UL << 58) /* Record fault */

/* Word 3 - VTTBR for stage-2 translation */
#define STE_3_S2TTB_MASK (0xfffffffffffcUL) /* VTTBR bits [51:4] */

/* Encode STE word 2 for a given VMID with 4KB granule, 40-bit PA, 39-bit IPA */
#define STE2_S2_ENCODE(vmid)                                                \
	(((uint64_t)(vmid) << STE_2_S2VMID_SHIFT) |                         \
	 (25UL << STE_2_S2T0SZ_SHIFT) | (1UL << STE_2_S2SL0_SHIFT) |        \
	 (1UL << STE_2_S2IR0_SHIFT) | (1UL << STE_2_S2OR0_SHIFT) |          \
	 STE_2_S2SH0_IS | STE_2_S2TG_4K | STE_2_S2PS_40BIT | STE_2_S2AA64 | \
	 STE_2_S2R)

/* -----------------------------------------------------------------------
 * Context Descriptor (CD) - used for stage-1 translation
 * Each CD is 64 bytes (8 × uint64_t).
 * ----------------------------------------------------------------------- */

#define CD_SIZE_BYTES 64

/* CD Word 0 */
#define CD_0_T0SZ_SHIFT 0
#define CD_0_T0SZ_48BIT (16UL << 0) /* 48-bit VA (T0SZ=16) */
#define CD_0_TG0_4K (0UL << 6) /* 4KB granule */
#define CD_0_ENDI (1UL << 15) /* Endianness override */
#define CD_0_EPD0 (1UL << 14) /* Fault on TTBR0 table walk */
#define CD_0_VALID (1UL << 31) /* CD is valid */
#define CD_0_ASID_SHIFT 48

/* -----------------------------------------------------------------------
 * SMMU MMIO accessors
 * ----------------------------------------------------------------------- */

static inline uint32_t smmu_read32(uintptr_t base, uint32_t offset)
{
	return *(volatile uint32_t *)(base + offset);
}

static inline void smmu_write32(uintptr_t base, uint32_t offset, uint32_t val)
{
	*(volatile uint32_t *)(base + offset) = val;
}

static inline uint64_t smmu_read64(uintptr_t base, uint32_t offset)
{
	return *(volatile uint64_t *)(base + offset);
}

static inline void smmu_write64(uintptr_t base, uint32_t offset, uint64_t val)
{
	*(volatile uint64_t *)(base + offset) = val;
}

/* Poll CR0ACK until it matches CR0 value (command accepted) */
static inline void smmu_wait_cr0ack(uintptr_t base, uint32_t cr0_val)
{
	while ((smmu_read32(base, SMMU_CR0ACK) & CR0_TRANSLATIONEN) !=
	       (cr0_val & CR0_TRANSLATIONEN))
		__asm__ volatile("yield");
}

#endif /* HAVEN_ASM_SMMU_H */
