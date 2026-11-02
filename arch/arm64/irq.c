/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/irq.c - EL2 IRQ virtualization via GICv3 List Registers.
 *
 * Implements the hardware path for virtual IRQ injection into guests.
 * The policy layer (src/core/exc/el2_exceptions.c) calls these after
 * validating IRQ ownership.
 *
 * GICv3 LR (List Register) layout (ICH_LR<n>_EL2):
 *   Bits [63:62] - State: 00=Invalid, 01=Pending, 10=Active, 11=Active+Pending
 *   Bit  [60]   - Group: 0=Group0, 1=Group1
 *   Bits [41:32] - pINTID (physical IRQ ID, for hardware IRQs)
 *   Bits [55:48] - Priority (GICv3 spec: ICH_LR<n>_EL2[55:48])
 *   Bits [9:0]  - vINTID (virtual IRQ ID sent to guest)
 */

#include <asm/sysregs.h>
#include <haven/types.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * LR encoding helpers - local to this file.
 * Use distinct prefix (HV_LR_) to avoid collision with sysregs.h ICH_LR_*.
 * Priority at bits [55:48] per GICv3 architecture reference (IHI0069).
 * ----------------------------------------------------------------------- */

#define HV_LR_STATE_MASK (3ULL << 62)
#define HV_LR_STATE_INVALID (0ULL << 62)
#define HV_LR_STATE_PENDING (1ULL << 62)
#define HV_LR_GROUP1 (1ULL << 60)
#define HV_LR_PRIORITY(p) (((uint64_t)(p) & 0xFFU) << 48)
#define HV_LR_VINTID(v) ((uint64_t)(v) & 0x3FFU)

static inline uint64_t make_lr(uint32_t virq, uint32_t priority)
{
	return HV_LR_STATE_PENDING | HV_LR_GROUP1 | HV_LR_PRIORITY(priority) |
	       HV_LR_VINTID(virq);
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
	case 0:
		__asm__ volatile("mrs %0, ich_lr0_el2" : "=r"(val));
		break;
	case 1:
		__asm__ volatile("mrs %0, ich_lr1_el2" : "=r"(val));
		break;
	case 2:
		__asm__ volatile("mrs %0, ich_lr2_el2" : "=r"(val));
		break;
	case 3:
		__asm__ volatile("mrs %0, ich_lr3_el2" : "=r"(val));
		break;
	case 4:
		__asm__ volatile("mrs %0, ich_lr4_el2" : "=r"(val));
		break;
	case 5:
		__asm__ volatile("mrs %0, ich_lr5_el2" : "=r"(val));
		break;
	case 6:
		__asm__ volatile("mrs %0, ich_lr6_el2" : "=r"(val));
		break;
	case 7:
		__asm__ volatile("mrs %0, ich_lr7_el2" : "=r"(val));
		break;
	case 8:
		__asm__ volatile("mrs %0, ich_lr8_el2" : "=r"(val));
		break;
	case 9:
		__asm__ volatile("mrs %0, ich_lr9_el2" : "=r"(val));
		break;
	case 10:
		__asm__ volatile("mrs %0, ich_lr10_el2" : "=r"(val));
		break;
	case 11:
		__asm__ volatile("mrs %0, ich_lr11_el2" : "=r"(val));
		break;
	case 12:
		__asm__ volatile("mrs %0, ich_lr12_el2" : "=r"(val));
		break;
	case 13:
		__asm__ volatile("mrs %0, ich_lr13_el2" : "=r"(val));
		break;
	case 14:
		__asm__ volatile("mrs %0, ich_lr14_el2" : "=r"(val));
		break;
	case 15:
		__asm__ volatile("mrs %0, ich_lr15_el2" : "=r"(val));
		break;
	default:
		break;
	}

	return val;
}

static void write_ich_lr(unsigned int n, uint64_t val)
{
	switch (n) {
	case 0:
		__asm__ volatile("msr ich_lr0_el2,  %0" : : "r"(val));
		break;
	case 1:
		__asm__ volatile("msr ich_lr1_el2,  %0" : : "r"(val));
		break;
	case 2:
		__asm__ volatile("msr ich_lr2_el2,  %0" : : "r"(val));
		break;
	case 3:
		__asm__ volatile("msr ich_lr3_el2,  %0" : : "r"(val));
		break;
	case 4:
		__asm__ volatile("msr ich_lr4_el2,  %0" : : "r"(val));
		break;
	case 5:
		__asm__ volatile("msr ich_lr5_el2,  %0" : : "r"(val));
		break;
	case 6:
		__asm__ volatile("msr ich_lr6_el2,  %0" : : "r"(val));
		break;
	case 7:
		__asm__ volatile("msr ich_lr7_el2,  %0" : : "r"(val));
		break;
	case 8:
		__asm__ volatile("msr ich_lr8_el2,  %0" : : "r"(val));
		break;
	case 9:
		__asm__ volatile("msr ich_lr9_el2,  %0" : : "r"(val));
		break;
	case 10:
		__asm__ volatile("msr ich_lr10_el2, %0" : : "r"(val));
		break;
	case 11:
		__asm__ volatile("msr ich_lr11_el2, %0" : : "r"(val));
		break;
	case 12:
		__asm__ volatile("msr ich_lr12_el2, %0" : : "r"(val));
		break;
	case 13:
		__asm__ volatile("msr ich_lr13_el2, %0" : : "r"(val));
		break;
	case 14:
		__asm__ volatile("msr ich_lr14_el2, %0" : : "r"(val));
		break;
	case 15:
		__asm__ volatile("msr ich_lr15_el2, %0" : : "r"(val));
		break;
	default:
		break;
	}
}

/* -----------------------------------------------------------------------
 * hv_arch_irq_enable - clear DAIF.I, enabling physical IRQs at EL2.
 * hv_arch_irq_disable - set DAIF.I, masking physical IRQs at EL2.
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
 * hv_arch_gic_el2_setup - configure EL2 virtual CPU interface.
 *
 * Programs:
 *   ICH_HCR_EL2  - enable virtual CPU interface (En = 1)
 *   ICH_VMCR_EL2 - guest view: Group1 enabled, FIQ bypass off,
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
   *   VFIQBYP = 0 (FIQ bypass disabled - routed through LRs)
   *   VIRQBYP = 0 (IRQ bypass disabled - routed through LRs)
   *   VPMR [31:24] = 0xFF (lowest priority mask: all interrupts pass)
   */
	uint64_t vmcr = ICH_VMCR_VENG1 | (0xFFUL << ICH_VMCR_VPMR_SHIFT);
	write_sysreg(vmcr, ich_vmcr_el2);
	isb();
}

/* -----------------------------------------------------------------------
 * Physical interrupts taken at EL2 (HCR_EL2.IMO = 1).
 *
 * hv_arch_phys_irq_cpu_init opens the calling CPU's interface for Group 1
 * interrupts (ICC_PMR_EL1 = 0xF0 lets every priority above the lowest
 * through; ICC_IGRPEN1_EL1 = 1) and sets ICC_CTLR_EL1.EOImode = 1, so that
 * ICC_EOIR1_EL1 only drops priority and deactivation is separate.  The boot
 * CPU calls it through hv_arch_phys_irq_init; a secondary CPU calls it
 * before entering its partition.
 *
 * hv_arch_handle_phys_irq acknowledges each pending interrupt:
 *   - Haven's own (the SMMU event queue): handled, then EOI and DIR;
 *   - owned by the partition running on this CPU: injected as a virtual
 *     interrupt linked to the physical one (ICH_LR HW = 1), then EOI only;
 *     the partition's own EOI deactivates the physical interrupt.  The
 *     owner is checked against the running partition, not inferred from
 *     the routing, so a misrouted interrupt never reaches another
 *     partition;
 *   - anything else, or no free list register: EOI and DIR.  An SPI that
 *     cannot be delivered is also disabled in the distributor: a level
 *     interrupt nobody clears would otherwise fire again at once and keep
 *     the CPU in Haven (observed in QEMU before this rule).  A PPI is
 *     private to its CPU, so its owner is looked up for (INTID, this CPU)
 *     and an undeliverable PPI is disabled on this CPU only.
 * ----------------------------------------------------------------------- */

static uint32_t smmu_evtq_intid;

/* Partition running on each CPU (0: none), set just before Haven enters
 * it.  An interrupt is injected only into its owner. */
#define HV_ARCH_IRQ_MAX_CPUS 16U
static hv_u32 cpu_partition[HV_ARCH_IRQ_MAX_CPUS];

void hv_arch_set_cpu_partition(uint32_t cpu, hv_u32 partition)
{
	if (cpu < HV_ARCH_IRQ_MAX_CPUS)
		cpu_partition[cpu] = partition;
}

#define ICC_CTLR_EOIMODE (1UL << 1)

/* Open this CPU's physical interface: EOImode 1, every priority above the
 * lowest, Group 1 enabled.  Each CPU that runs a partition calls it. */
void hv_arch_phys_irq_cpu_init(void)
{
	write_sysreg(read_sysreg(icc_ctlr_el1) | ICC_CTLR_EOIMODE,
		     icc_ctlr_el1);
	write_sysreg(0xF0UL, icc_pmr_el1);
	write_sysreg(1UL, icc_igrpen1_el1);
	isb();
}

void hv_arch_phys_irq_init(uint32_t smmu_intid)
{
	smmu_evtq_intid = smmu_intid;
	hv_arch_phys_irq_cpu_init();
}

/* Queue a virtual interrupt linked to physical intid (same number) in a free
 * list register.  Returns HV_OK or HV_ENOSPC. */
static hv_status_t inject_hw(uint32_t intid, uint32_t priority)
{
	for (unsigned int i = 0; i < 16U; i++) {
		if ((read_ich_lr(i) & HV_LR_STATE_MASK) != HV_LR_STATE_INVALID)
			continue;
		write_ich_lr(i,
			     HV_LR_STATE_PENDING | ICH_LR_HW | HV_LR_GROUP1 |
				     HV_LR_PRIORITY(priority) |
				     ((uint64_t)intid << ICH_LR_PINTID_SHIFT) |
				     HV_LR_VINTID(intid));
		isb();
		return HV_OK;
	}
	return HV_ENOSPC;
}

void hv_arch_handle_phys_irq(void)
{
	extern void hv_printk(const char *fmt, ...);
	extern void smmu_v3_service_irq(void);
	extern uint32_t hv_arch_cpu_id(void);
	extern hv_u32 hv_irq_owner_on(hv_u32 irq_id, hv_u32 cpu);
	extern hv_status_t gic_v3_disable_irq(uint32_t irq);
	extern hv_status_t gic_v3_disable_ppi(uint32_t cpu, uint32_t irq);

	for (;;) {
		uint64_t iar = read_sysreg(icc_iar1_el1);
		uint32_t intid = (uint32_t)(iar & 0xffffffU);
		hv_u32 owner;
		uint32_t here = hv_arch_cpu_id();

		if (intid >= 1020U && intid <= 1023U)
			break; /* spurious or none pending */
		if (intid == smmu_evtq_intid && intid != 0U) {
			smmu_v3_service_irq();
		} else {
			owner = hv_irq_owner_on(intid, here);
			if (owner != 0U && here < HV_ARCH_IRQ_MAX_CPUS &&
			    owner == cpu_partition[here] &&
			    inject_hw(intid, 0xa0U) == HV_OK) {
				write_sysreg(iar,
					     icc_eoir1_el1); /* drop only */
				isb();
				continue;
			}
			if (intid >= 32U) {
				(void)gic_v3_disable_irq(intid);
				hv_printk("HAVEN: IRQ %u not delivered, "
					  "disabled\n",
					  (unsigned)intid);
			} else if (intid >= 16U) {
				/* A PPI is private: disable it on this
				 * CPU only (a level timer would storm). */
				(void)gic_v3_disable_ppi(here, intid);
				hv_printk("HAVEN: PPI %u on CPU %u not "
					  "delivered, disabled\n",
					  (unsigned)intid, (unsigned)here);
			}
		}
		write_sysreg(iar, icc_eoir1_el1);
		write_sysreg(iar, icc_dir_el1);
		isb();
	}
}

/* -----------------------------------------------------------------------
 * hv_arch_inject_virtual_irq - inject a virtual IRQ into the current guest.
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
 * hv_arch_pending_virtual_irqs - count LRs in Pending or Active+Pending state.
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
