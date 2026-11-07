/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Emulated GICv3 distributor and redistributor for one vCPU (L3).
 *
 * The partition's CPU interface is the hardware's virtual one (ICV system
 * registers, list registers filled by HAVEN); only the memory-mapped
 * distributor (GICD) and redistributor (GICR: RD and SGI frames) are
 * emulated here. Register offsets and fields: Arm IHI 0069 (GICv3/v4
 * architecture specification); what Linux reads at probe:
 * drivers/irqchip/irq-gic-v3.c.
 *
 * The model is pure: the hardware is reached only through the hooks.
 *
 * @file include/haven/vgic.h
 */

#ifndef HAVEN_VGIC_H
#define HAVEN_VGIC_H

#include <haven/types.h>

#define HV_VGIC_NR_IRQS 64U /* INTIDs 0..63: 16 SGIs, 16 PPIs, 32 SPIs */
#define HV_VGIC_GICD_SIZE 0x10000U
#define HV_VGIC_GICR_SIZE 0x20000U /* RD frame + SGI frame */
#define HV_VGIC_DEFAULT_PRIO 0xA0U

struct hv_vgic_hooks {
	/* 1 if the partition owns INTID on its CPU (PPIs and SPIs) */
	int (*owned)(hv_u32 cpu, hv_u32 intid);
	/* enable (on = 1) or disable the physical interrupt */
	void (*hw_enable)(hv_u32 cpu, hv_u32 intid, int on);
};

struct hv_vgic {
	hv_u32 cpu;
	hv_u64 affinity; /* MPIDR Aff3..Aff0 as GICR_TYPER[63:32] wants */
	struct hv_vgic_hooks hooks;
	hv_u32 gicd_ctlr;
	hv_u32 waker;
	hv_u32 enabled[HV_VGIC_NR_IRQS / 32U];
	hv_u32 group[HV_VGIC_NR_IRQS / 32U];
	hv_u32 grpmod[HV_VGIC_NR_IRQS / 32U];
	hv_u32 cfg[HV_VGIC_NR_IRQS / 16U];
	hv_u8 prio[HV_VGIC_NR_IRQS];
	hv_u64 irouter[HV_VGIC_NR_IRQS];
	hv_u32 last_frame; /* GICR_TYPER.Last: the last redistributor frame */
};

/** Reset the model for the vCPU on `cpu` and physically disable every
 * interrupt the partition owns there until the guest enables it. */
void hv_vgic_init(struct hv_vgic *v, hv_u32 cpu, hv_u64 mpidr,
		  const struct hv_vgic_hooks *hooks);

/** MMIO handlers for hv_mmio_register(): ctx is the struct hv_vgic. */
int hv_vgic_gicd_access(void *ctx, hv_u64 off, hv_u32 size, int write,
			hv_u64 *val);
int hv_vgic_gicr_access(void *ctx, hv_u64 off, hv_u32 size, int write,
			hv_u64 *val);

/** A write of `value` to ICC_SGI1R_EL1 by the vCPU: returns the SGI to
 * inject into this vCPU (0..15), or -1 if it targets no enabled SGI on
 * this vCPU (other CPUs are dropped: one vCPU). */
int hv_vgic_sgi(const struct hv_vgic *v, hv_u64 value);

/** The vCPUs of one multi-vCPU partition, for SGIs between them (n = 0:
 * none). The array is kept, not copied. */
void hv_vgic_set_peers(struct hv_vgic *const *peers, hv_u32 n);

/** The peers registered with v, and their number in *n, or NULL. */
struct hv_vgic *const *hv_vgic_peers(const struct hv_vgic *v, hv_u32 *n);

/** A write of `value` to ICC_SGI1R_EL1 by `sender`: the bit mask (bit i =
 * peers[i]) of the vCPUs of the same partition it targets and that have
 * the SGI enabled; *intid is set to the SGI. IRM = 1 targets every peer
 * but the sender. */
hv_u32 hv_vgic_sgi_targets(const struct hv_vgic *sender, hv_u64 value,
			   struct hv_vgic *const *peers, hv_u32 n,
			   hv_u32 *intid);

/** Model registered for `cpu`, or NULL. */
struct hv_vgic *hv_vgic_for_cpu(hv_u32 cpu);

#endif /* HAVEN_VGIC_H */
