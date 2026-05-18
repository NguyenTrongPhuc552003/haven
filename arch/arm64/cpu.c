/* SPDX-License-Identifier: Apache-2.0 */
/* ARM64 EL2 CPU initialization for the Haven hypervisor. */

#include <asm/sysregs.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * hv_arch_cpu_init - configure EL2 control registers on the current CPU.
 *
 * Must be called once per CPU before any guest activity.
 * Configures:
 *   - HCR_EL2:  enable stage-2 translation, IRQ/FIQ/SError routing to EL2
 *   - SCTLR_EL2: enable I/D caches at EL2 (Haven code cached)
 *   - VTCR_EL2:  39-bit IPA space, 4KB granule, 3-level walk
 *   - VTTBR_EL2: VMID=0 (no-translation initial state)
 *   - MDCR_EL2:  no debug/PMU traps
 *   - CPTR_EL2:  allow FP/NEON in guests (TFP=0)
 *   - HSTR_EL2:  no system register traps
 *   - ICC_SRE_EL2: enable GICv3 system register interface
 * ----------------------------------------------------------------------- */
void hv_arch_cpu_init(void) {
  uint64_t hcr;
  uint64_t sctlr;

  /* HCR_EL2: VM=1 (stage-2 on), RW=1 (EL1 AArch64),
   * IMO/FMO/AMO=1 (route virtual exceptions to EL2),
   * TSC=1 (trap SMC to EL2 so guests can't bypass us). */
  hcr = HCR_HAVEN_DEFAULT;
  write_sysreg(hcr, hcr_el2);
  isb();

  /* VTCR_EL2: 39-bit IPA, 4KB, 3-level walk, 40-bit PA,
   * IRGN/ORGN = WB RA WA (inner/outer write-back), IS shareable */
  write_sysreg(VTCR_HAVEN_DEFAULT, vtcr_el2);

  /* VTTBR_EL2: start with VMID=0, no translation table
   * (will be set per-partition at context switch time). */
  write_sysreg(0, vttbr_el2);

  /* SCTLR_EL2: enable I and D caches at EL2.
   * SP alignment check off (hypervisor uses explicit alignment).
   * WXN off (hypervisor code is not writable). */
  sctlr = read_sysreg(sctlr_el2);
  sctlr |= SCTLR_EL2_I | SCTLR_EL2_C | SCTLR_EL2_RES1;
  sctlr &= ~(SCTLR_EL2_A | SCTLR_EL2_SA | SCTLR_EL2_WXN);
  write_sysreg(sctlr, sctlr_el2);

  /* MDCR_EL2: no PMU/debug traps; let guests manage their own PMU. */
  write_sysreg(0, mdcr_el2);

  /* CPTR_EL2: TFP=0 so FP/SIMD is available to guests without trapping.
   * TZ=0 so SVE is available if present. RES1 bits must stay set. */
  write_sysreg(CPTR_EL2_RES1, cptr_el2);

  /* HSTR_EL2: no system register traps (cp15 accessible at EL1). */
  write_sysreg(HSTR_EL2_NONE, hstr_el2);

  /* ICC_SRE_EL2: enable GICv3 CPU interface system registers,
   * disable FIQ/IRQ bypass, allow EL1 to use ICC_SRE_EL1. */
  write_sysreg(ICC_SRE_EL2_SRE | ICC_SRE_EL2_DFB | ICC_SRE_EL2_DIB |
                   ICC_SRE_EL2_ENABLE,
               icc_sre_el2);
  isb();

  /* Enable virtual CPU interface and set default virtual machine control. */
  write_sysreg(ICH_HCR_EN, ich_hcr_el2);
  write_sysreg(ICH_VMCR_DEFAULT, ich_vmcr_el2);

  /* Install exception vector table */
  extern void hv_install_vectors(void);
  hv_install_vectors();

  isb();
}

/* -----------------------------------------------------------------------
 * hv_arch_cpu_id - return the current CPU logical index.
 *
 * Reads MPIDR_EL1 Aff0 field (CPU number within cluster).
 * Assumes linear CPU numbering (Aff1=cluster, Aff0=cpu within cluster).
 * ----------------------------------------------------------------------- */
uint32_t hv_arch_cpu_id(void) {
  uint64_t mpidr = read_sysreg(mpidr_el1);
  /* Return (Aff1 * 4 + Aff0) as logical CPU index */
  uint32_t aff0 = (mpidr >> 0) & 0xff;
  uint32_t aff1 = (mpidr >> 8) & 0xff;
  return (aff1 << 2) | (aff0 & 0x3);
}

/* -----------------------------------------------------------------------
 * hv_arch_cpu_mpidr - return raw MPIDR value for this CPU.
 *
 * Used when programming GICD_IROUTER for direct IRQ routing.
 * ----------------------------------------------------------------------- */
uint64_t hv_arch_cpu_mpidr(void) {
  return read_sysreg(mpidr_el1) & 0x00ffffffU;
}

/* -----------------------------------------------------------------------
 * hv_arch_wfi - wait for interrupt (CPU idle loop).
 * ----------------------------------------------------------------------- */
void hv_arch_wfi(void) { __asm__ volatile("wfi" : : : "memory"); }

/* -----------------------------------------------------------------------
 * hv_arch_nop - explicit NOP for spinloop delays.
 * ----------------------------------------------------------------------- */
void hv_arch_nop(void) { __asm__ volatile("yield" : : : "memory"); }
