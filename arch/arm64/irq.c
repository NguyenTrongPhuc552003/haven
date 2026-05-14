/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/irq.c — EL2 IRQ virtualization via GICv3 List Registers.
 *
 * Implements the hardware path for virtual IRQ injection into guests.
 * The policy layer (src/core/exc/el2_exceptions.c) calls these after
 * validating IRQ ownership.
 *
 * GICv3 LR (List Register) layout (ICH_LR<n>_EL2):
 *   Bits [63:62] — State: 00=Invalid, 01=Pending, 10=Active, 11=Active+Pending
 *   Bit  [60]   — Group: 0=Group0, 1=Group1
 *   Bits [41:32] — pINTID (physical IRQ ID, for hardware IRQs)
 *   Bits [55:48] — Priority (GICv3 spec: ICH_LR<n>_EL2[55:48])
 *   Bits [9:0]  — vINTID (virtual IRQ ID sent to guest)
 */

#include <stdint.h>
#include <asm/sysregs.h>
#include <haven/types.h>

/* -----------------------------------------------------------------------
 * LR encoding helpers — local to this file.
 * Use distinct prefix (HV_LR_) to avoid collision with sysregs.h ICH_LR_*.
 * Priority at bits [55:48] per GICv3 architecture reference (IHI0069).
 * ----------------------------------------------------------------------- */

#define HV_LR_STATE_MASK        (3ULL << 62)
#define HV_LR_STATE_INVALID     (0ULL << 62)
#define HV_LR_STATE_PENDING     (1ULL << 62)
#define HV_LR_GROUP1            (1ULL << 60)
#define HV_LR_PRIORITY(p)       (((uint64_t)(p) & 0xFFU) << 48)
#define HV_LR_VINTID(v)         ((uint64_t)(v) & 0x3FFU)

static inline uint64_t make_lr(uint32_t virq, uint32_t priority)
{
        return HV_LR_STATE_PENDING | HV_LR_GROUP1 |
               HV_LR_PRIORITY(priority) | HV_LR_VINTID(virq);
}

/* -----------------------------------------------------------------------
 * read_ich_lr / write_ich_lr
 *
 * GICv3 list registers are discrete system registers; they cannot be
 * indexed via a variable.  A switch statement is the only portable way
 * to select among ICH_LR0_EL2 .. ICH_LR15_EL2 at runtime.
 * ----------------------------------------------------------------------- */

static uint64_t read_ich_lr(unsigned int n)
{
        uint64_t val = 0;

        switch (n) {
        case  0: __asm__ volatile("mrs %0, ich_lr0_el2"  : "=r"(val)); break;
        case  1: __asm__ volatile("mrs %0, ich_lr1_el2"  : "=r"(val)); break;
        case  2: __asm__ volatile("mrs %0, ich_lr2_el2"  : "=r"(val)); break;
        case  3: __asm__ volatile("mrs %0, ich_lr3_el2"  : "=r"(val)); break;
        case  4: __asm__ volatile("mrs %0, ich_lr4_el2"  : "=r"(val)); break;
        case  5: __asm__ volatile("mrs %0, ich_lr5_el2"  : "=r"(val)); break;
        case  6: __asm__ volatile("mrs %0, ich_lr6_el2"  : "=r"(val)); break;
        case  7: __asm__ volatile("mrs %0, ich_lr7_el2"  : "=r"(val)); break;
        case  8: __asm__ volatile("mrs %0, ich_lr8_el2"  : "=r"(val)); break;
        case  9: __asm__ volatile("mrs %0, ich_lr9_el2"  : "=r"(val)); break;
        case 10: __asm__ volatile("mrs %0, ich_lr10_el2" : "=r"(val)); break;
        case 11: __asm__ volatile("mrs %0, ich_lr11_el2" : "=r"(val)); break;
        case 12: __asm__ volatile("mrs %0, ich_lr12_el2" : "=r"(val)); break;
        case 13: __asm__ volatile("mrs %0, ich_lr13_el2" : "=r"(val)); break;
        case 14: __asm__ volatile("mrs %0, ich_lr14_el2" : "=r"(val)); break;
        case 15: __asm__ volatile("mrs %0, ich_lr15_el2" : "=r"(val)); break;
        default: break;
        }

        return val;
}

static void write_ich_lr(unsigned int n, uint64_t val)
{
        switch (n) {
        case  0: __asm__ volatile("msr ich_lr0_el2,  %0" : : "r"(val)); break;
        case  1: __asm__ volatile("msr ich_lr1_el2,  %0" : : "r"(val)); break;
        case  2: __asm__ volatile("msr ich_lr2_el2,  %0" : : "r"(val)); break;
        case  3: __asm__ volatile("msr ich_lr3_el2,  %0" : : "r"(val)); break;
        case  4: __asm__ volatile("msr ich_lr4_el2,  %0" : : "r"(val)); break;
        case  5: __asm__ volatile("msr ich_lr5_el2,  %0" : : "r"(val)); break;
        case  6: __asm__ volatile("msr ich_lr6_el2,  %0" : : "r"(val)); break;
        case  7: __asm__ volatile("msr ich_lr7_el2,  %0" : : "r"(val)); break;
        case  8: __asm__ volatile("msr ich_lr8_el2,  %0" : : "r"(val)); break;
        case  9: __asm__ volatile("msr ich_lr9_el2,  %0" : : "r"(val)); break;
        case 10: __asm__ volatile("msr ich_lr10_el2, %0" : : "r"(val)); break;
        case 11: __asm__ volatile("msr ich_lr11_el2, %0" : : "r"(val)); break;
        case 12: __asm__ volatile("msr ich_lr12_el2, %0" : : "r"(val)); break;
        case 13: __asm__ volatile("msr ich_lr13_el2, %0" : : "r"(val)); break;
        case 14: __asm__ volatile("msr ich_lr14_el2, %0" : : "r"(val)); break;
        case 15: __asm__ volatile("msr ich_lr15_el2, %0" : : "r"(val)); break;
        default: break;
        }
}

/* -----------------------------------------------------------------------
 * hv_arch_irq_enable — clear DAIF.I, enabling physical IRQs at EL2.
 * hv_arch_irq_disable — set DAIF.I, masking physical IRQs at EL2.
 *
 * DAIFClr/DAIFSet immediate field: bit 1 = I (IRQ mask).
 * ----------------------------------------------------------------------- */

void hv_arch_irq_enable(void)
{
        __asm__ volatile("msr daifclr, #2" : : : "memory");
}

void hv_arch_irq_disable(void)
{
        __asm__ volatile("msr daifset, #2" : : : "memory");
}

/* -----------------------------------------------------------------------
 * hv_arch_gic_el2_setup — configure EL2 virtual CPU interface.
 *
 * Programs:
 *   ICH_HCR_EL2  — enable virtual CPU interface (En = 1)
 *   ICH_VMCR_EL2 — guest view: Group1 enabled, FIQ bypass off,
 *                  AckCtl=0, CBPR=0, priority mask=0xFF (all unmasked)
 *
 * Called once per CPU during hypervisor bringup (after hv_arch_cpu_init).
 * ----------------------------------------------------------------------- */

void hv_arch_gic_el2_setup(void)
{
        /* Enable the virtual CPU interface. */
        write_sysreg(ICH_HCR_EN, ich_hcr_el2);
        isb();

        /*
         * ICH_VMCR_EL2 default for guests:
         *   VENG1 = 1  (Group 1 virtual interrupts enabled)
         *   VENG0 = 0  (Group 0 / secure FIQ disabled for normal guests)
         *   VACKCTL = 0 (no EOI-mode split)
         *   VCBPR = 0   (guest uses own BPR, not BPR0)
         *   VFIQBYP = 0 (FIQ bypass disabled — routed through LRs)
         *   VIRQBYP = 0 (IRQ bypass disabled — routed through LRs)
         *   VPMR [31:24] = 0xFF (lowest priority mask: all interrupts pass)
         */
        uint64_t vmcr = ICH_VMCR_VENG1 |
                        (0xFFUL << ICH_VMCR_VPMR_SHIFT);
        write_sysreg(vmcr, ich_vmcr_el2);
        isb();
}

/* -----------------------------------------------------------------------
 * hv_arch_inject_virtual_irq — inject a virtual IRQ into the current guest.
 *
 * Scans ICH_LR0_EL2 .. ICH_LR15_EL2 for an Invalid slot, writes the LR
 * with State=Pending, Group1, and the requested priority and vINTID.
 *
 * @virq:     virtual INTID to deliver to the guest (bits [9:0])
 * @priority: GIC priority byte; 0x00 = highest, 0xFF = lowest
 *
 * Returns HV_OK if the IRQ was enqueued, HV_ENOSPC if all 16 LRs are busy.
 * ----------------------------------------------------------------------- */

hv_status_t hv_arch_inject_virtual_irq(uint32_t virq, uint32_t priority)
{
        unsigned int i;

        for (i = 0; i < 16U; i++) {
                uint64_t lr = read_ich_lr(i);

                if ((lr & HV_LR_STATE_MASK) == HV_LR_STATE_INVALID) {
                        write_ich_lr(i, make_lr(virq, priority));
                        isb();
                        return HV_OK;
                }
        }

        return HV_ENOSPC;
}

/* -----------------------------------------------------------------------
 * hv_arch_pending_virtual_irqs — count LRs in Pending or Active+Pending state.
 *
 * Used by the EL2 maintenance ISR to determine whether the guest still has
 * outstanding virtual interrupts before returning control.
 * ----------------------------------------------------------------------- */

uint32_t hv_arch_pending_virtual_irqs(void)
{
        uint32_t count = 0;
        unsigned int i;

        for (i = 0; i < 16U; i++) {
                uint64_t lr = read_ich_lr(i);
                uint64_t state = (lr >> 62) & 0x3ULL;

                /*
                 * state == 1 (Pending) or state == 3 (Active+Pending).
                 * Both have bit 0 of the 2-bit state field set.
                 */
                if (state & 1ULL)
                        count++;
        }

        return count;
}
