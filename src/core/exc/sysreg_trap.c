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
/* SGI for a vCPU on another CPU: queue it there (arch/arm64/irq.c). */
extern void hv_arch_post_virtual_sgi(uint32_t cpu, uint32_t intid)
	__attribute__((weak));

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

/* SGIs `ready` (bit = SGI) for vCPU t, sent from vCPU `here`: injected
 * locally when t is the vCPU on this CPU, else posted to t's CPU. */
void hv_vgic_deliver_sgis(const struct hv_vgic *here, const struct hv_vgic *t,
			  hv_u32 ready)
{
	for (hv_u32 id = 0; id < 16U; id++) {
		if ((ready & (1U << id)) == 0U)
			continue;
		if (t == here && hv_arch_inject_virtual_irq != NULL)
			(void)hv_arch_inject_virtual_irq(id,
							 HV_VIRT_IRQ_PRIORITY);
		else if (t != here && hv_arch_post_virtual_sgi != NULL)
			hv_arch_post_virtual_sgi(t->cpu, id);
	}
}

void hv_handle_sysreg_trap(hv_u64 esr, hv_u64 *frame, hv_u32 cpu)
{
	static unsigned logged;
	hv_u32 iss = (hv_u32)(esr & 0x1FFFFFFU);
	hv_u32 rt = (iss >> 5) & 0x1FU;
	int read = (int)(iss & 1U);

	if ((iss & SYSREG_KEY_MASK) == SYSREG_ICC_SGI1R && !read) {
		struct hv_vgic *v = hv_vgic_for_cpu(cpu);
		struct hv_vgic *const *peers;
		hv_u32 n = 0U, sgi_id, mask;
		int sgi;

		peers = v ? hv_vgic_peers(v, &n) : NULL;
		if (peers != NULL) { /* a vCPU of several (multi-vcpu.md) */
			mask = hv_vgic_sgi_addressed(v, hv_frame_get(frame, rt),
						     peers, n, &sgi_id);
			for (hv_u32 i = 0; i < n; i++) {
				hv_u32 ready;

				if ((mask & (1U << i)) == 0U)
					continue;
				/* Disabled there: kept pending (SMP3) */
				ready = (__atomic_load_n(&peers[i]->enabled[0],
							 __ATOMIC_RELAXED) &
					 (1U << sgi_id)) != 0U ?
						(1U << sgi_id) :
						hv_vgic_sgi_hold(peers[i],
								 sgi_id);
				hv_vgic_deliver_sgis(v, peers[i], ready);
			}
		} else {
			sgi = v ? hv_vgic_sgi(v, hv_frame_get(frame, rt)) : -1;
			if (sgi >= 0 && hv_arch_inject_virtual_irq != NULL)
				(void)hv_arch_inject_virtual_irq(
					(uint32_t)sgi, HV_VIRT_IRQ_PRIORITY);
		}
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
