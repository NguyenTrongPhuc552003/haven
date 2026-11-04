/* SPDX-License-Identifier: Apache-2.0 */
/* ARM64 EL2 system register read/write accessors for the Haven hypervisor. */

#ifndef HAVEN_ASM_SYSREGS_H
#define HAVEN_ASM_SYSREGS_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Generic system register accessors
 * ----------------------------------------------------------------------- */

#define read_sysreg(r)                                                     \
	({                                                                 \
		uint64_t _val;                                             \
		__asm__ volatile("mrs %0, " #r : "=r"(_val) : : "memory"); \
		_val;                                                      \
	})

#define write_sysreg(v, r) \
	__asm__ volatile("msr " #r ", %0" : : "r"((uint64_t)(v)) : "memory")

#define read_sysreg_s(r)                                                  \
	({                                                                \
		uint64_t _val;                                            \
		__asm__ volatile("mrs %0, " r : "=r"(_val) : : "memory"); \
		_val;                                                     \
	})

#define write_sysreg_s(v, r) \
	__asm__ volatile("msr " r ", %0" : : "r"((uint64_t)(v)) : "memory")

/* -----------------------------------------------------------------------
 * EL2 virtual memory control
 * ----------------------------------------------------------------------- */

/* HCR_EL2 - Hypervisor Configuration Register */
#define HCR_VM (UINT64_C(1) << 0) /* Enable stage-2 translation */
#define HCR_SWIO (UINT64_C(1) << 1) /* Set/Way Invalidation Override */
#define HCR_PTW (UINT64_C(1) << 2) /* Protected Table Walk */
#define HCR_FMO (UINT64_C(1) << 3) /* FIQ routing to EL2 */
#define HCR_IMO (UINT64_C(1) << 4) /* IRQ routing to EL2 */
#define HCR_AMO (UINT64_C(1) << 5) /* SError routing to EL2 */
#define HCR_VF (UINT64_C(1) << 6) /* Virtual FIQ */
#define HCR_VI (UINT64_C(1) << 7) /* Virtual IRQ */
#define HCR_VSE (UINT64_C(1) << 8) /* Virtual SError */
#define HCR_FB (UINT64_C(1) << 9) /* Force Broadcast */
#define HCR_BSU_IS \
	(UINT64_C(2) << 10) /* Barrier Shareability: Inner Shareable */
#define HCR_DC (UINT64_C(1) << 12) /* Default cacheable */
#define HCR_TWI (UINT64_C(1) << 13) /* Trap WFI */
#define HCR_TWE (UINT64_C(1) << 14) /* Trap WFE */
#define HCR_TID0 (UINT64_C(1) << 15) /* Trap ID group 0 */
#define HCR_TID3 (UINT64_C(1) << 18) /* Trap ID group 3 */
#define HCR_TSC (UINT64_C(1) << 19) /* Trap SMC */
#define HCR_TIDCP (UINT64_C(1) << 20) /* Trap IMPLEMENTATION DEFINED */
#define HCR_TACR (UINT64_C(1) << 21) /* Trap Auxiliary Control Registers */
#define HCR_TSW (UINT64_C(1) << 22) /* Trap Set/Way cache ops */
#define HCR_TPCP (UINT64_C(1) << 23) /* Trap DC CVAP/CVADP */
#define HCR_TPU (UINT64_C(1) << 24) /* Trap Cache Maintenance ops to PoU */
#define HCR_TTLB (UINT64_C(1) << 25) /* Trap TLB maintenance */
#define HCR_TVM (UINT64_C(1) << 26) /* Trap Virtual Memory controls */
#define HCR_TGE (UINT64_C(1) << 27) /* Trap General Exceptions */
#define HCR_TDZ (UINT64_C(1) << 28) /* Trap DC ZVA */
#define HCR_HCD (UINT64_C(1) << 29) /* Hypervisor Call Disable */
#define HCR_TRVM (UINT64_C(1) << 30) /* Trap reads of VM controls */
#define HCR_RW (UINT64_C(1) << 31) /* Register Width: 1 = EL1 is AArch64 */
#define HCR_CD (UINT64_C(1) << 32) /* Cache Disable */
#define HCR_ID (UINT64_C(1) << 33) /* I-Cache Disable */
#define HCR_E2H (UINT64_C(1) << 34) /* EL2 Host (VHE) */
#define HCR_TLOR (UINT64_C(1) << 35) /* Trap LOR registers */
#define HCR_TERR (UINT64_C(1) << 36) /* Trap Error record access */
#define HCR_TEA (UINT64_C(1) << 37) /* Route External Abort to EL2 */
#define HCR_MIOCNCE  \
	(UINT64_C(1) \
	 << 38) /* Mismatched Inner/Outer Cacheable Non-Coherency Enable */
#define HCR_APK (UINT64_C(1) << 40) /* Disable key traps */
#define HCR_API (UINT64_C(1) << 41) /* Disable PAC instruction traps */
#define HCR_NV (UINT64_C(1) << 42) /* Nested Virtualization */
#define HCR_NV1 (UINT64_C(1) << 43) /* Nested Virtualization 1 */
#define HCR_AT (UINT64_C(1) << 44) /* Address Translation */
#define HCR_NV2 (UINT64_C(1) << 45) /* Nested Virtualization 2 */
#define HCR_FWB (UINT64_C(1) << 46) /* Forced Write-Back */
#define HCR_FIEN (UINT64_C(1) << 47) /* Fault Injection Enable */
#define HCR_TID4 (UINT64_C(1) << 49) /* Trap ID group 4 */
#define HCR_TICAB (UINT64_C(1) << 50) /* Trap IC IALLU/IALLUIS */
#define HCR_AMVOFFEN \
	(UINT64_C(1) << 51) /* Activity Monitor Virtual Offset Enable */
#define HCR_TOCU (UINT64_C(1) << 52) /* Trap cache ops to PoU */
#define HCR_ENSCXT \
	(UINT64_C(1) << 53) /* Enable EL0 and EL1 accesses to SCXTNUM */
#define HCR_TTLBIS \
	(UINT64_C(1) << 54) /* Trap TLB Invalidate Instruction Sharing */
#define HCR_TTLBOS (UINT64_C(1) << 55) /* Trap TLB Invalidate Outer Sharing */
#define HCR_ATA (UINT64_C(1) << 56) /* Allocation Tag Access */
#define HCR_DCT (UINT64_C(1) << 57) /* Default Cacheability Tagging */
#define HCR_TID5 (UINT64_C(1) << 58) /* Trap ID group 5 */
#define HCR_TWEDEL (UINT64_C(4) << 59) /* TWE Delay: 16 µs */
#define HCR_TWEDEN (UINT64_C(1) << 63) /* TWE Delay Enable */

/* Haven default HCR_EL2: VM=1, RW=1, IMO=1, FMO=1, AMO=1, TSC=1 */
#define HCR_HAVEN_DEFAULT \
	(HCR_VM | HCR_RW | HCR_IMO | HCR_FMO | HCR_AMO | HCR_TSC)

/* VTCR_EL2 - Virtualization Translation Control Register */
#define VTCR_T0SZ(x) ((x) & 0x3f) /* IPA size: T0SZ=25 → 39-bit */
#define VTCR_SL0(x) (((x) & 0x3) << 6) /* Starting level */
#define VTCR_IRGN0(x) (((x) & 0x3) << 8) /* Inner cacheability */
#define VTCR_ORGN0(x) (((x) & 0x3) << 10) /* Outer cacheability */
#define VTCR_SH0(x) (((x) & 0x3) << 12) /* Shareability */
#define VTCR_TG0_4K (UINT64_C(0) << 14) /* 4KB granule */
#define VTCR_TG0_64K (UINT64_C(1) << 14) /* 64KB granule */
#define VTCR_PS(x) (((x) & 0x7) << 16) /* Physical address size */
#define VTCR_VS (UINT64_C(1) << 19) /* VMID size: 1 = 16-bit */
#define VTCR_HA (UINT64_C(1) << 21) /* Hardware access flag update */
#define VTCR_HD (UINT64_C(1) << 22) /* Hardware dirty flag update */
#define VTCR_RES1 (UINT64_C(1) << 31) /* Reserved, must be 1 */

/* VTCR_EL2 for 40-bit PA, 4KB granule, 3-level walk (IPA = 39 bit) */
#define VTCR_HAVEN_DEFAULT                                             \
	(VTCR_T0SZ(25) | VTCR_SL0(1) | VTCR_IRGN0(1) | VTCR_ORGN0(1) | \
	 VTCR_SH0(3) | VTCR_TG0_4K | VTCR_PS(2) | VTCR_RES1)

/* VTTBR_EL2 - Virtualization Translation Table Base Register */
#define VTTBR_VMID_SHIFT 48
#define VTTBR_VMID_MASK (UINT64_C(0xffff) << VTTBR_VMID_SHIFT)
#define VTTBR_BADDR_MASK (UINT64_C(0xffffffffffff))
#define vttbr_encode(baddr, vmid) \
	(((baddr) & VTTBR_BADDR_MASK) | ((uint64_t)(vmid) << VTTBR_VMID_SHIFT))

/* -----------------------------------------------------------------------
 * EL2 configuration registers
 * ----------------------------------------------------------------------- */

/* SCTLR_EL2 - System Control Register EL2 */
#define SCTLR_EL2_M (UINT64_C(1) << 0) /* MMU Enable */
#define SCTLR_EL2_A (UINT64_C(1) << 1) /* Alignment Check */
#define SCTLR_EL2_C (UINT64_C(1) << 2) /* Data cache Enable */
#define SCTLR_EL2_SA (UINT64_C(1) << 3) /* Stack Alignment Check */
#define SCTLR_EL2_I (UINT64_C(1) << 12) /* Instruction cache Enable */
#define SCTLR_EL2_WXN (UINT64_C(1) << 19) /* Write Execute Never */
#define SCTLR_EL2_EE (UINT64_C(1) << 25) /* Exception Endianness */
#define SCTLR_EL2_RES1                                                     \
	((UINT64_C(1) << 4) | (UINT64_C(1) << 5) | (UINT64_C(1) << 11) |   \
	 (UINT64_C(1) << 16) | (UINT64_C(1) << 18) | (UINT64_C(1) << 22) | \
	 (UINT64_C(1) << 23) | (UINT64_C(1) << 28) | (UINT64_C(1) << 29))

/* MDCR_EL2 - Monitor Debug Configuration Register EL2 */
#define MDCR_HPME (UINT64_C(1) << 7) /* Enable EL2 performance monitors */
#define MDCR_TPM (UINT64_C(1) << 6) /* Trap PMU accesses */
#define MDCR_TPMCR (UINT64_C(1) << 5) /* Trap PMCR_EL0 */
#define MDCR_HPMN_MASK (0x1f) /* Hypervisor PMN */

/* CPTR_EL2 - Architectural Feature Trap Register EL2 */
#define CPTR_EL2_TFP (UINT64_C(1) << 10) /* Trap FP/SIMD at EL1/EL0 */
#define CPTR_EL2_TZ (UINT64_C(1) << 8) /* Trap SVE */
#define CPTR_EL2_RES1 ((UINT64_C(1) << 13) | (UINT64_C(0xff) << 0))

/* HSTR_EL2 - Hypervisor System Trap Register */
#define HSTR_EL2_T15 (UINT64_C(1) << 15) /* Trap CP15 */
#define HSTR_EL2_NONE (UINT64_C(0)) /* No system register traps */

/* -----------------------------------------------------------------------
 * EL2 exception registers
 * ----------------------------------------------------------------------- */

/* ESR_EL2 - Exception Syndrome Register EL2 */
#define ESR_ELx_EC_SHIFT 26
#define ESR_ELx_EC_MASK (UINT64_C(0x3f) << ESR_ELx_EC_SHIFT)
#define ESR_ELx_EC(esr) (((esr) & ESR_ELx_EC_MASK) >> ESR_ELx_EC_SHIFT)
#define ESR_ELx_IL (UINT64_C(1) << 25) /* Instruction Length */
#define ESR_ELx_ISS_MASK (UINT64_C(0x1ffffff))

/* Exception Class values */
#define ESR_EC_UNKNOWN 0x00
#define ESR_EC_WF 0x01 /* WFI/WFE trapped */
#define ESR_EC_CP15_32 0x03 /* CP15 MCR/MRC */
#define ESR_EC_CP15_64 0x04 /* CP15 MCRR/MRRC */
#define ESR_EC_CP14_MR 0x05 /* CP14 MCR/MRC */
#define ESR_EC_CP14_LS 0x06 /* CP14 LDC/STC */
#define ESR_EC_FP_ASIMD 0x07 /* FP/ASIMD access */
#define ESR_EC_SYS64 0x18 /* MSR/MRS/SYS in AArch64 */
#define ESR_EC_SVE 0x19 /* SVE access */
#define ESR_EC_HVC64 0x16 /* HVC in AArch64 */
#define ESR_EC_SMC64 0x17 /* SMC in AArch64 */
#define ESR_EC_IABT_LOW 0x20 /* Instruction Abort from lower EL */
#define ESR_EC_IABT_CUR 0x21 /* Instruction Abort from current EL */
#define ESR_EC_PC_ALIGN 0x22 /* PC alignment fault */
#define ESR_EC_DABT_LOW 0x24 /* Data Abort from lower EL */
#define ESR_EC_DABT_CUR 0x25 /* Data Abort from current EL */
#define ESR_EC_SP_ALIGN 0x26 /* SP alignment fault */
#define ESR_EC_FP32 0x28 /* FP exception from AArch32 */
#define ESR_EC_FP64 0x2c /* FP exception from AArch64 */
#define ESR_EC_SERROR 0x2f /* SError interrupt */
#define ESR_EC_BREAKPT_LOW 0x30 /* Breakpoint from lower EL */
#define ESR_EC_BREAKPT_CUR 0x31 /* Breakpoint from current EL */
#define ESR_EC_SOFTSTP_LOW 0x32 /* Software Step from lower EL */
#define ESR_EC_SOFTSTP_CUR 0x33 /* Software Step from current EL */
#define ESR_EC_WATCHPT_LOW 0x34 /* Watchpoint from lower EL */
#define ESR_EC_WATCHPT_CUR 0x35 /* Watchpoint from current EL */
#define ESR_EC_BKPT32 0x38 /* BKPT in AArch32 */
#define ESR_EC_BRK64 0x3c /* BRK in AArch64 */

/* Data/Instruction Abort ISS fields */
#define ESR_ISS_DABT_ISV (UINT64_C(1) << 24) /* Instruction Syndrome Valid */
#define ESR_ISS_DABT_WNR (UINT64_C(1) << 6) /* Write, Not Read */
#define ESR_ISS_DABT_DFSC (UINT64_C(0x3f)) /* Data Fault Status Code */
#define ESR_ISS_DABT_DFSC_TRANS_L0 0x04 /* Translation fault level 0 */
#define ESR_ISS_DABT_DFSC_TRANS_L1 0x05 /* Translation fault level 1 */
#define ESR_ISS_DABT_DFSC_TRANS_L2 0x06 /* Translation fault level 2 */
#define ESR_ISS_DABT_DFSC_TRANS_L3 0x07 /* Translation fault level 3 */
#define ESR_ISS_DABT_DFSC_PERM_L3 0x0f /* Permission fault level 3 */

/* HVC immediate value: ESR_ELx bits [15:0] */
#define ESR_ISS_HVC_IMM(esr) ((esr) & UINT64_C(0xffff))

/* -----------------------------------------------------------------------
 * EL2 timer registers
 * ----------------------------------------------------------------------- */

/* CNTHP_CTL_EL2 - EL2 Physical Timer Control */
#define CNTHP_CTL_ENABLE (UINT64_C(1) << 0) /* Timer enable */
#define CNTHP_CTL_IMASK (UINT64_C(1) << 1) /* Interrupt mask */
#define CNTHP_CTL_ISTATUS \
	(UINT64_C(1) << 2) /* Timer status (deadline passed) */

/* CNTVOFF_EL2 - Counter-timer Virtual Offset Register */
/* Used to present a virtualized counter value to each guest */

/* -----------------------------------------------------------------------
 * GICv3 system register interface (ICH / ICC at EL2)
 * ----------------------------------------------------------------------- */

/* ICC_SRE_EL2 - System Register Enable Register (EL2) */
#define ICC_SRE_EL2_SRE (UINT64_C(1) << 0) /* System Register Enable */
#define ICC_SRE_EL2_DFB (UINT64_C(1) << 1) /* Disable FIQ bypass */
#define ICC_SRE_EL2_DIB (UINT64_C(1) << 2) /* Disable IRQ bypass */
#define ICC_SRE_EL2_ENABLE \
	(UINT64_C(1) << 3) /* Allow EL1 to change ICC_SRE_EL1 */

/* ICH_HCR_EL2 - Interrupt Controller Hypervisor Control Register */
#define ICH_HCR_EN (UINT64_C(1) << 0) /* Virtual CPU interface enable */
#define ICH_HCR_UIE (UINT64_C(1) << 1) /* Underflow Interrupt Enable */
#define ICH_HCR_LRENPIE \
	(UINT64_C(1) << 2) /* List Register Entry Not Present IE */
#define ICH_HCR_NPIE (UINT64_C(1) << 3) /* No Pending Interrupt Enable */
#define ICH_HCR_VGRP0EIE (UINT64_C(1) << 4) /* VM Group 0 Enabled IE */
#define ICH_HCR_VGRP0DIE (UINT64_C(1) << 5) /* VM Group 0 Disabled IE */
#define ICH_HCR_VGRP1EIE (UINT64_C(1) << 6) /* VM Group 1 Enabled IE */
#define ICH_HCR_VGRP1DIE (UINT64_C(1) << 7) /* VM Group 1 Disabled IE */
#define ICH_HCR_TC (UINT64_C(1) << 10) /* Trap EL1 writes to GICV EOI */
#define ICH_HCR_TALL0 (UINT64_C(1) << 11) /* Trap all EL1 Group-0 accesses */
#define ICH_HCR_TALL1 (UINT64_C(1) << 12) /* Trap all EL1 Group-1 accesses */
#define ICH_HCR_TSEI (UINT64_C(1) << 13) /* Trap all EL1 SEI maintenance */
#define ICH_HCR_TDIR (UINT64_C(1) << 14) /* Trap EL1 DIR writes */
#define ICH_HCR_EOIcount_SHIFT 27
#define ICH_HCR_EOIcount_MASK (UINT64_C(0x1f) << ICH_HCR_EOIcount_SHIFT)

/* ICH_LR<n>_EL2 - List Register format */
#define ICH_LR_VINTID_MASK (UINT64_C(0xffff)) /* Virtual INTID [15:0] */
#define ICH_LR_PINTID_SHIFT 32
#define ICH_LR_PINTID_MASK \
	(UINT64_C(0x3ff) << ICH_LR_PINTID_SHIFT) /* Physical INTID */
#define ICH_LR_PRIORITY_SHIFT 48
#define ICH_LR_PRIORITY_MASK (UINT64_C(0xff) << ICH_LR_PRIORITY_SHIFT)
#define ICH_LR_GROUP (UINT64_C(1) << 60) /* Group: 0=Grp0, 1=Grp1 */
#define ICH_LR_HW (UINT64_C(1) << 61) /* HW=1: physical INTID mapped */
#define ICH_LR_STATE_SHIFT 62
#define ICH_LR_STATE_INVALID (UINT64_C(0) << 62)
#define ICH_LR_STATE_PENDING (UINT64_C(1) << 62)
#define ICH_LR_STATE_ACTIVE (UINT64_C(2) << 62)
#define ICH_LR_STATE_PENDACT (UINT64_C(3) << 62)

#define ich_lr_make_pending(vintid, pintid, prio)          \
	(ICH_LR_STATE_PENDING | ICH_LR_GROUP | ICH_LR_HW | \
	 ((uint64_t)(prio) << ICH_LR_PRIORITY_SHIFT) |     \
	 ((uint64_t)(pintid) << ICH_LR_PINTID_SHIFT) |     \
	 ((vintid) & ICH_LR_VINTID_MASK))

/* ICH_VMCR_EL2 - Virtual Machine Control Register */
#define ICH_VMCR_VENG0 (UINT64_C(1) << 0) /* Virtual Group 0 enable */
#define ICH_VMCR_VENG1 (UINT64_C(1) << 1) /* Virtual Group 1 enable */
#define ICH_VMCR_VACKCTL (UINT64_C(1) << 2) /* Virtual AckCtl */
#define ICH_VMCR_VFIQBYP (UINT64_C(1) << 3) /* Virtual FIQ Bypass */
#define ICH_VMCR_VIRQBYP (UINT64_C(1) << 4) /* Virtual IRQ Bypass */
#define ICH_VMCR_VEOIM (UINT64_C(1) << 9) /* Virtual EOI Mode */
#define ICH_VMCR_VPMR_SHIFT 24
#define ICH_VMCR_VPMR_MASK (UINT64_C(0xff) << ICH_VMCR_VPMR_SHIFT)
#define ICH_VMCR_DEFAULT                   \
	(ICH_VMCR_VENG0 | ICH_VMCR_VENG1 | \
	 (UINT64_C(0xf0) << ICH_VMCR_VPMR_SHIFT))

/* ICH_MISR_EL2 - Maintenance Interrupt Status Register */
#define ICH_MISR_EOI (UINT64_C(1) << 0)
#define ICH_MISR_U (UINT64_C(1) << 1)
#define ICH_MISR_LRENP (UINT64_C(1) << 2)
#define ICH_MISR_NP (UINT64_C(1) << 3)
#define ICH_MISR_VGRP0E (UINT64_C(1) << 4)
#define ICH_MISR_VGRP0D (UINT64_C(1) << 5)
#define ICH_MISR_VGRP1E (UINT64_C(1) << 6)
#define ICH_MISR_VGRP1D (UINT64_C(1) << 7)

/* ICC_IAR1_EL1 read at EL2: pending IRQ ack */
#define ICC_IAR1_INTID_MASK (UINT64_C(0x1ffffff))
#define ICC_IAR1_SPURIOUS 0x3ff /* No pending IRQ */

/* -----------------------------------------------------------------------
 * TLB invalidation instructions
 * ----------------------------------------------------------------------- */

/* Invalidate all stage-2 translations for current VMID, Inner Shareable */
#define tlbi_vmalls12e1is() __asm__ volatile("tlbi vmalls12e1is" : : : "memory")

/* Invalidate all stage-1/2 translations, all VMIDs, Inner Shareable */
#define tlbi_alle2is() __asm__ volatile("tlbi alle2is" : : : "memory")

/* Invalidate by IPA (stage-2), Inner Shareable. addr >> 12. */
#define tlbi_ipas2e1is(ipa) \
	__asm__ volatile("tlbi ipas2e1is, %0" : : "r"((ipa) >> 12) : "memory")

/* Data Synchronization Barrier (full system) */
#define dsb_sy() __asm__ volatile("dsb sy" : : : "memory")

/* Data Synchronization Barrier (inner shareable) */
#define dsb_ish() __asm__ volatile("dsb ish" : : : "memory")

/* Instruction Synchronization Barrier */
#define isb() __asm__ volatile("isb" : : : "memory")

#endif /* HAVEN_ASM_SYSREGS_H */
