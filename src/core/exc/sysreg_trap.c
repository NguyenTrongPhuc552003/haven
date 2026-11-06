/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Trapped system register accesses (EC 0x18), moved out of
 * el2_exceptions.c unchanged so that host unit tests can run it.
 *
 * @file src/core/exc/sysreg_trap.c
 */

#include <haven/el2_exceptions.h>
#include <haven/mmio.h>
#include <haven/sysreg_trap.h>
#include <haven/vgic.h>

extern hv_status_t hv_arch_inject_virtual_irq(uint32_t virq, uint32_t priority)
	__attribute__((weak));
extern void hv_printk(const char *fmt, ...) __attribute__((weak));

/* ISS: Op0 [21:20], Op2 [19:17], Op1 [16:14], CRn [13:10], Rt [9:5],
 * CRm [4:1], read [0]. ICC_SGI1R_EL1 (3, 0, C12, C11, 5) writes trap
 * because HCR_EL2.IMO is set: a self-targeted SGI is injected as a
 * virtual SGI (one vCPU). Anything else reads as zero and ignores
 * writes, logged a few times. */
#define SYSREG_KEY(op0, op1, crn, crm, op2)              \
	(((hv_u32)(op0) << 20) | ((hv_u32)(op2) << 17) | \
	 ((hv_u32)(op1) << 14) | ((hv_u32)(crn) << 10) | ((hv_u32)(crm) << 1))
#define SYSREG_KEY_MASK 0x3FFC1EU
#define SYSREG_ICC_SGI1R SYSREG_KEY(3, 0, 12, 11, 5)

void hv_handle_sysreg_trap(hv_u64 esr, hv_u64 *frame, hv_u32 cpu)
{
	static unsigned logged;
	hv_u32 iss = (hv_u32)(esr & 0x1FFFFFFU);
	hv_u32 rt = (iss >> 5) & 0x1FU;
	int read = (int)(iss & 1U);

	if ((iss & SYSREG_KEY_MASK) == SYSREG_ICC_SGI1R && !read) {
		struct hv_vgic *v = hv_vgic_for_cpu(cpu);
		int sgi = v ? hv_vgic_sgi(v, hv_frame_get(frame, rt)) : -1;

		if (sgi >= 0 && hv_arch_inject_virtual_irq != NULL)
			(void)hv_arch_inject_virtual_irq((uint32_t)sgi,
							 HV_VIRT_IRQ_PRIORITY);
	} else {
		if (read)
			hv_frame_set(frame, rt, 0U);
		if (logged < 4U && hv_printk) {
			logged++;
			hv_printk("HAVEN: trapped system register access "
				  "ISS=0x%x (%s) cpu=%u\n",
				  (unsigned)iss,
				  read ? "reads as zero" : "write ignored",
				  (unsigned)cpu);
		}
	}
	frame[HV_FRAME_ELR] += 4ULL;
}
