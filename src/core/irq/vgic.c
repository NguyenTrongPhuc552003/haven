/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Emulated GICv3 distributor and redistributor for one vCPU (L3).
 *
 * Reports 64 interrupt IDs (32 SPIs), security disabled (GICD_CTLR.DS = 1),
 * no LPIs and no extended SPIs. Enable and disable of an interrupt the
 * partition owns go to the physical GIC; other interrupt IDs read as zero
 * and ignore writes, except SGIs (virtual state only). Pending and active
 * state are not emulated (read as zero, writes ignored). Priority,
 * configuration, group and routing are stored and read back, not applied
 * to the hardware.
 *
 * @file src/core/irq/vgic.c
 */

#include <haven/vgic.h>

/* GICD */
#define GICD_CTLR 0x0000U
#define GICD_TYPER 0x0004U
#define GICD_IGROUPR 0x0080U
#define GICD_ISENABLER 0x0100U
#define GICD_ICENABLER 0x0180U
#define GICD_IPRIORITYR 0x0400U
#define GICD_ICFGR 0x0C00U
#define GICD_IGRPMODR 0x0D00U
#define GICD_IROUTER 0x6000U
#define GIC_PIDR2 0xFFE8U

#define GICD_CTLR_DS (1U << 6)
#define GICD_CTLR_WRITABLE 0x37U /* EnableGrp0/1NS/1S, ARE_S, ARE_NS */
/* ITLinesNumber = 1 (64 IDs), IDbits = 9 (10 bits) */
#define GICD_TYPER_VALUE ((9U << 19) | 1U)
#define GIC_PIDR2_V3 0x30U

/* GICR: RD frame, then SGI frame at +64 KiB */
#define GICR_CTLR 0x0000U
#define GICR_TYPER 0x0008U
#define GICR_WAKER 0x0014U
#define GICR_SGI 0x10000U
#define GICR_WAKER_PS (1U << 1) /* ProcessorSleep */
#define GICR_WAKER_CA (1U << 2) /* ChildrenAsleep */
#define GICR_TYPER_LAST (1ULL << 4)

/* Read-back (tools/readback/readback.py, v8) finds each model through the
 * symbol by_cpu and reads enabled[] at this offset; keep them in step. */
_Static_assert(__builtin_offsetof(struct hv_vgic, enabled) == 40,
	       "readback.py reads hv_vgic.enabled at offset 40");

static struct hv_vgic *by_cpu[16];

struct hv_vgic *hv_vgic_for_cpu(hv_u32 cpu)
{
	return cpu < 16U ? by_cpu[cpu] : NULL;
}

void hv_vgic_init(struct hv_vgic *v, hv_u32 cpu, hv_u64 mpidr,
		  const struct hv_vgic_hooks *hooks)
{
	hv_u8 *p = (hv_u8 *)v;

	for (hv_u32 i = 0; i < sizeof(*v); i++)
		p[i] = 0;
	v->cpu = cpu;
	v->hooks = *hooks;
	v->affinity = (((mpidr >> 32) & 0xFFU) << 24) | (mpidr & 0xFFFFFFU);
	v->gicd_ctlr = GICD_CTLR_DS;
	v->waker = GICR_WAKER_PS | GICR_WAKER_CA; /* asleep at reset */
	v->cfg[0] = 0xAAAAAAAAU; /* SGIs are edge-triggered */
	v->last_frame = 1U;
	for (hv_u32 id = 16; id < HV_VGIC_NR_IRQS; id++)
		if (v->hooks.owned(cpu, id))
			v->hooks.hw_enable(cpu, id, 0);
	if (cpu < 16U)
		by_cpu[cpu] = v;
}

/* Set or clear enable bits for 32 IDs from word n. */
static void set_enable(struct hv_vgic *v, hv_u32 n, hv_u32 bits, int on)
{
	for (hv_u32 b = 0; b < 32U; b++) {
		hv_u32 id = n * 32U + b;
		hv_u32 m = 1U << b;

		if ((bits & m) == 0U || id >= HV_VGIC_NR_IRQS)
			continue;
		if (id >= 16U) {
			if (!v->hooks.owned(v->cpu, id))
				continue; /* not this partition's: WI */
			if (((v->enabled[n] & m) != 0U) != on)
				v->hooks.hw_enable(v->cpu, id, on);
		}
		if (on)
			v->enabled[n] |= m;
		else
			v->enabled[n] &= ~m;
	}
}

/* Bit-per-ID register block at offset `off` from its base, for words
 * [first, last]. Returns 1 if off fell in the block. */
static int bitmap(hv_u32 *store, hv_u64 off, hv_u32 base, hv_u32 first,
		  hv_u32 last, int write, hv_u64 *val)
{
	hv_u32 n;

	if (off < base || off >= base + 0x80U)
		return 0;
	n = (hv_u32)(off - base) / 4U;
	if (n < first || n > last) {
		if (!write)
			*val = 0; /* RAZ/WI outside the IDs this view owns */
		return 1;
	}
	if (write)
		store[n] = (hv_u32)*val;
	else
		*val = store[n];
	return 1;
}

/* Byte-per-ID priorities for IDs [lo, hi). */
static void prio_access(struct hv_vgic *v, hv_u32 id0, hv_u32 size, int write,
			hv_u64 *val, hv_u32 lo, hv_u32 hi)
{
	hv_u64 r = 0;

	for (hv_u32 i = 0; i < size; i++) {
		hv_u32 id = id0 + i;
		int in = id >= lo && id < hi;

		if (write && in)
			v->prio[id] = (hv_u8)(*val >> (8U * i));
		else if (!write && in)
			r |= (hv_u64)v->prio[id] << (8U * i);
	}
	if (!write)
		*val = r;
}

/* Common block of the GICD (words 1..) and the GICR SGI frame (word 0). */
static int irq_regs(struct hv_vgic *v, hv_u64 off, hv_u32 size, int write,
		    hv_u64 *val, hv_u32 w0, hv_u32 w1)
{
	hv_u32 n;

	if (off >= GICD_ISENABLER && off < GICD_ICENABLER + 0x80U) {
		int set = off < GICD_ICENABLER;

		n = (hv_u32)(off - (set ? GICD_ISENABLER : GICD_ICENABLER)) /
		    4U;
		if (n < w0 || n > w1) {
			if (!write)
				*val = 0;
		} else if (write) {
			set_enable(v, n, (hv_u32)*val, set);
		} else {
			*val = v->enabled[n];
		}
		return 0;
	}
	if (bitmap(v->group, off, GICD_IGROUPR, w0, w1, write, val) ||
	    bitmap(v->grpmod, off, GICD_IGRPMODR, w0, w1, write, val))
		return 0;
	if (off >= GICD_IPRIORITYR && off < GICD_IPRIORITYR + 0x400U) {
		prio_access(v, (hv_u32)(off - GICD_IPRIORITYR), size, write,
			    val, w0 * 32U, (w1 + 1U) * 32U);
		return 0;
	}
	if (off >= GICD_ICFGR && off < GICD_ICFGR + 0x100U) {
		/* two bits per ID: word n holds IDs 16n..16n+15 */
		n = (hv_u32)(off - GICD_ICFGR) / 4U;
		if (n < w0 * 2U || n > w1 * 2U + 1U) {
			if (!write)
				*val = 0; /* not this view's IDs */
		} else if (write) {
			if (n != 0U) /* SGIs stay edge-triggered */
				v->cfg[n] = (hv_u32)*val;
		} else {
			*val = v->cfg[n];
		}
		return 0;
	}
	if (!write)
		*val = 0; /* pending, active, anything else: RAZ/WI */
	return 0;
}

int hv_vgic_gicd_access(void *ctx, hv_u64 off, hv_u32 size, int write,
			hv_u64 *val)
{
	struct hv_vgic *v = ctx;

	switch (off) {
	case GICD_CTLR:
		if (write)
			v->gicd_ctlr = ((hv_u32)*val & GICD_CTLR_WRITABLE) |
				       GICD_CTLR_DS;
		else
			*val = v->gicd_ctlr; /* RWP reads 0 */
		return 0;
	case GICD_TYPER:
		if (!write)
			*val = GICD_TYPER_VALUE;
		return 0;
	case GIC_PIDR2:
		if (!write)
			*val = GIC_PIDR2_V3;
		return 0;
	default:
		break;
	}
	if (off >= GICD_IROUTER + 32U * 8U &&
	    off < GICD_IROUTER + HV_VGIC_NR_IRQS * 8U) {
		hv_u32 id = (hv_u32)(off - GICD_IROUTER) / 8U;
		hv_u32 hi = (off & 4U) != 0U; /* upper half, 32-bit access */
		hv_u64 mask = size == 8U ? ~0ULL : 0xFFFFFFFFULL;
		hv_u32 sh = hi ? 32U : 0U;

		if (write)
			v->irouter[id] = (v->irouter[id] & ~(mask << sh)) |
					 ((*val & mask) << sh);
		else
			*val = (v->irouter[id] >> sh) & mask;
		return 0;
	}
	/* SGIs and PPIs live in the redistributor (affinity routing). */
	return irq_regs(v, off, size, write, val, 1U,
			HV_VGIC_NR_IRQS / 32U - 1U);
}

int hv_vgic_gicr_access(void *ctx, hv_u64 off, hv_u32 size, int write,
			hv_u64 *val)
{
	struct hv_vgic *v = ctx;

	if (off >= GICR_SGI)
		return irq_regs(v, off - GICR_SGI, size, write, val, 0U, 0U);

	switch (off) {
	case GICR_TYPER: /* 64-bit, or the low half */
		if (!write) {
			hv_u64 t = (v->affinity << 32) |
				   (v->last_frame ? GICR_TYPER_LAST : 0U);
			*val = size == 8U ? t : (t & 0xFFFFFFFFULL);
		}
		return 0;
	case GICR_TYPER + 4U:
		if (!write)
			*val = v->affinity & 0xFFFFFFFFULL;
		return 0;
	case GICR_WAKER:
		if (write)
			v->waker = ((hv_u32)*val & GICR_WAKER_PS) != 0U ?
					   (GICR_WAKER_PS | GICR_WAKER_CA) :
					   0U;
		else
			*val = v->waker;
		return 0;
	case GIC_PIDR2:
		if (!write)
			*val = GIC_PIDR2_V3;
		return 0;
	case GICR_CTLR: /* no LPIs; RWP reads 0 */
	default:
		if (!write)
			*val = 0;
		return 0;
	}
}

int hv_vgic_sgi(const struct hv_vgic *v, hv_u64 value)
{
	hv_u32 intid = (hv_u32)((value >> 24) & 0xFU);
	hv_u32 aff0 = (hv_u32)(v->affinity & 0xFFU);
	hv_u64 aff123 = ((value >> 16) & 0xFFU) | /* Aff1 */
			(((value >> 32) & 0xFFU) << 8) | /* Aff2 */
			(((value >> 48) & 0xFFU) << 16); /* Aff3 */

	if ((value >> 40) & 1U)
		return -1; /* IRM: every CPU but the sender */
	if (aff123 != ((v->affinity >> 8) & 0xFFFFFFU))
		return -1;
	if (((value >> 44) & 0xFU) != aff0 / 16U) /* RS */
		return -1;
	if (((value >> (aff0 % 16U)) & 1U) == 0U)
		return -1;
	if ((v->enabled[0] & (1U << intid)) == 0U)
		return -1;
	return (int)intid;
}

static struct hv_vgic *const *peer_set;
static hv_u32 peer_count;

void hv_vgic_set_peers(struct hv_vgic *const *peers, hv_u32 n)
{
	peer_set = peers;
	peer_count = peers != NULL ? n : 0U;
}

struct hv_vgic *const *hv_vgic_peers(const struct hv_vgic *v, hv_u32 *n)
{
	for (hv_u32 i = 0; i < peer_count; i++)
		if (peer_set[i] == v) {
			*n = peer_count;
			return peer_set;
		}
	*n = 0U;
	return NULL;
}

hv_u32 hv_vgic_sgi_addressed(const struct hv_vgic *sender, hv_u64 value,
			     struct hv_vgic *const *peers, hv_u32 n,
			     hv_u32 *intid)
{
	hv_u32 id = (hv_u32)((value >> 24) & 0xFU);
	hv_u64 aff123 = ((value >> 16) & 0xFFU) | /* Aff1 */
			(((value >> 32) & 0xFFU) << 8) | /* Aff2 */
			(((value >> 48) & 0xFFU) << 16); /* Aff3 */
	int irm = (int)((value >> 40) & 1U);
	hv_u32 rs = (hv_u32)((value >> 44) & 0xFU);
	hv_u32 mask = 0U;

	*intid = id;
	for (hv_u32 i = 0; i < n && i < 32U; i++) {
		const struct hv_vgic *t = peers[i];
		hv_u32 aff0 = (hv_u32)(t->affinity & 0xFFU);

		if (irm) {
			if (t == sender)
				continue;
		} else if (aff123 != ((t->affinity >> 8) & 0xFFFFFFU) ||
			   rs != aff0 / 16U ||
			   ((value >> (aff0 % 16U)) & 1U) == 0U) {
			continue;
		}
		mask |= 1U << i;
	}
	return mask;
}

hv_u32 hv_vgic_sgi_targets(const struct hv_vgic *sender, hv_u64 value,
			   struct hv_vgic *const *peers, hv_u32 n,
			   hv_u32 *intid)
{
	hv_u32 mask = hv_vgic_sgi_addressed(sender, value, peers, n, intid);

	for (hv_u32 i = 0; i < n && i < 32U; i++)
		if ((peers[i]->enabled[0] & (1U << *intid)) == 0U)
			mask &= ~(1U << i);
	return mask;
}

/* Pending SGIs (SMP3): the sender sets the bit and then looks at the
 * enable; the vCPU that enables an SGI takes the pending ones after its
 * write. With sequentially consistent ordering at least one side sees
 * the other's write, and the atomic take delivers each SGI once. */
hv_u32 hv_vgic_sgi_take(struct hv_vgic *v)
{
	hv_u32 ready;

	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	ready = __atomic_load_n(&v->sgi_pending, __ATOMIC_RELAXED) &
		__atomic_load_n(&v->enabled[0], __ATOMIC_RELAXED) & 0xFFFFU;
	if (ready == 0U)
		return 0U;
	return __atomic_fetch_and(&v->sgi_pending, ~ready, __ATOMIC_SEQ_CST) &
	       ready;
}

hv_u32 hv_vgic_sgi_hold(struct hv_vgic *v, hv_u32 intid)
{
	if (intid >= 16U)
		return 0U;
	__atomic_fetch_or(&v->sgi_pending, 1U << intid, __ATOMIC_SEQ_CST);
	return hv_vgic_sgi_take(v);
}
