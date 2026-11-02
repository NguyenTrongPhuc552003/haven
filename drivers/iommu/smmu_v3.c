/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/iommu/smmu_v3.c - ARM SMMUv3 stream table and command queue driver.
 *
 * Implements the hardware binding for Haven's DMA isolation policy.
 * The policy layer (src/core/dma/smmu.c) validates ownership and calls
 * these functions to write Stream Table Entries (STEs) in hardware.
 *
 * Linear Stream Table (STE format type 0):
 *   - One 64-byte STE per StreamID.
 *   - STE.CONFIG selects: ABORT (deny), BYPASS (passthrough), S2_TRANS
 *     (isolate).
 *   - For S2_TRANS: STE.S2TTB points to a stage-2 table and STE.S2VMID tags
 *     its TLB entries.  The policy layer gives every DMA stream its own table
 *     that maps only the stream's window (arch/arm64/mm.c).
 *
 * Command queue: the SMMU caches STEs and translations, so every STE change
 * is followed by CFGI_STE (and TLB invalidation for the VMIDs involved) and a
 * CMD_SYNC.  The queue is a ring of 16-byte commands in memory; its address
 * and size go into SMMU_CMDQ_BASE, software advances SMMU_CMDQ_PROD and the
 * SMMU advances SMMU_CMDQ_CONS.  PROD and CONS hold an index in bits
 * [log2size-1:0] and a wrap flag in bit log2size, so (PROD - CONS) modulo
 * 2 * entries is the number of commands not yet consumed.
 *
 * Haven's isolation policy:
 *   - Unassigned StreamIDs -> ABORT STE (fail-closed default).
 *   - Assigned StreamIDs -> S2_TRANS STE.
 *   - Revocation replaces the S2_TRANS STE with an ABORT STE and invalidates.
 *
 * Every wait is bounded: a stuck or erroring SMMU makes the call return
 * HV_EIO instead of halting the hypervisor.
 *
 * References:
 *   ARM IHI 0070 - Arm System Memory Management Unit Architecture
 *   Specification, SMMU architecture version 3
 *   Field layouts cross-checked with Linux
 *   drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h
 */

#include <asm/smmu.h>
#include <haven/types.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Sizes
 * ----------------------------------------------------------------------- */

#define SMMU_STE_DWORDS 8 /* 8 x 8 bytes = 64 bytes per STE */
#define SMMU_MAX_STREAMS 256 /* Linear table size (Haven conservative limit) */

#define SMMU_CMDQ_LOG2 8U /* 256 commands x 16 bytes = 4 KiB */
#define SMMU_CMDQ_ENTRIES (1U << SMMU_CMDQ_LOG2)
#define SMMU_CMDQ_PTR_MASK ((SMMU_CMDQ_ENTRIES << 1) - 1U) /* index + wrap */
#define SMMU_CMDQ_IDX_MASK (SMMU_CMDQ_ENTRIES - 1U)

#define SMMU_EVTQ_LOG2 6U /* 64 events x 32 bytes = 2 KiB */
#define SMMU_EVTQ_ENTRIES (1U << SMMU_EVTQ_LOG2)

/* -----------------------------------------------------------------------
 * Driver state
 * ----------------------------------------------------------------------- */

static struct {
	uint64_t ste_table[SMMU_MAX_STREAMS * SMMU_STE_DWORDS]
		__attribute__((aligned(SMMU_MAX_STREAMS * 64)));
	/* The queue base must be aligned to the queue size. */
	uint64_t cmdq[SMMU_CMDQ_ENTRIES * CMDQ_ENT_DWORDS]
		__attribute__((aligned(SMMU_CMDQ_ENTRIES * 16)));
	/* Event queue: the SMMU records faults here (e.g. DMA outside a
	 * window).  Haven drains it on the event interrupt; consumed entries
	 * stay in memory, so external read-back still sees them. */
	uint64_t evtq[SMMU_EVTQ_ENTRIES * EVTQ_ENT_DWORDS]
		__attribute__((aligned(SMMU_EVTQ_ENTRIES * 32)));
	uintptr_t base;
	uint32_t cmdq_prod; /* software copy of SMMU_CMDQ_PROD */
	uint16_t ste_vmid[SMMU_MAX_STREAMS]; /* VMID of an S2 STE, 0 = none */
	uint32_t nr_streams;
	int s2; /* SMMU_IDR0.S2P: stage-2 translation implemented */
	int initialized;
} smmu;

/* -----------------------------------------------------------------------
 * STE construction helpers
 * ----------------------------------------------------------------------- */

/* Write an ABORT STE for a given StreamID: deny all DMA. */
static void ste_write_abort(uint32_t sid)
{
	uint64_t *ste = &smmu.ste_table[sid * SMMU_STE_DWORDS];

	/* Word 0: VALID=1, CONFIG=ABORT */
	ste[0] = STE_0_VALID | STE_0_CONFIG_ABORT;
	/* Words 1-7: zero (unused for ABORT) */
	for (int i = 1; i < SMMU_STE_DWORDS; i++)
		ste[i] = 0;

	/* Ensure store is visible before SMMU sees it */
	__asm__ volatile("dsb sy" ::: "memory");
}

/* Write a BYPASS STE for a StreamID: pass-through without translation.
 * Not used in isolation policy but provided for debug/testing. */
static void ste_write_bypass(uint32_t sid)
{
	uint64_t *ste = &smmu.ste_table[sid * SMMU_STE_DWORDS];

	ste[0] = STE_0_VALID | STE_0_CONFIG_BYPASS;
	for (int i = 1; i < SMMU_STE_DWORDS; i++)
		ste[i] = 0;

	__asm__ volatile("dsb sy" ::: "memory");
}

/* Write an S2_TRANS STE for a StreamID: stage-2 translation through the
 * table whose base is in vttbr, with TLB entries tagged by vmid. */
static void ste_write_s2(uint32_t sid, uint64_t vttbr, uint32_t vmid)
{
	uint64_t *ste = &smmu.ste_table[sid * SMMU_STE_DWORDS];

	/* Word 0: VALID + CONFIG_S2_TRANS */
	ste[0] = STE_0_VALID | STE_0_CONFIG_S2_TRANS;

	/* Word 1: no stage-1 context; keep the device's shareability */
	ste[1] = STE_1_SHCFG_INCOMING;

	/* Word 2: VMID and the VTCR-format walk parameters.
	 * STE2_S2_ENCODE encodes: VMID, T0SZ=25 (39-bit IPA),
	 * SL0=1 (level-1 start, as VTCR_EL2), IRGN=1 (WB), ORGN=1 (WB),
	 * SH=3 (IS), TG=0 (4KB), PS=2 (40-bit PA), AA64=1, S2R=1. */
	ste[2] = STE2_S2_ENCODE(vmid);

	/* Word 3: stage-2 table base, bits [51:4] (VMID and CnP dropped) */
	ste[3] = vttbr & STE_3_S2TTB_MASK;

	/* Words 4-7: reserved */
	for (int i = 4; i < SMMU_STE_DWORDS; i++)
		ste[i] = 0;

	__asm__ volatile("dsb sy" ::: "memory");
}

/* -----------------------------------------------------------------------
 * Command queue
 * ----------------------------------------------------------------------- */

/* Commands written but not yet consumed by the SMMU. */
static uint32_t cmdq_used(uint32_t prod, uint32_t cons)
{
	return (prod - cons) & SMMU_CMDQ_PTR_MASK;
}

/* Wait until the queue has room for [needed] commands.
 * Returns 0, or -1 on a queue error or timeout. */
static int cmdq_wait_space(uint32_t needed)
{
	for (uint32_t spin = 0; spin < SMMU_POLL_LIMIT; spin++) {
		uint32_t cons = smmu_read32(smmu.base, SMMU_CMDQ_CONS);

		if (cons & CMDQ_CONS_ERR_MASK)
			return -1;
		if (SMMU_CMDQ_ENTRIES - cmdq_used(smmu.cmdq_prod, cons) >=
		    needed)
			return 0;
		__asm__ volatile("yield" ::: "memory");
	}
	return -1;
}

/* Write one command into the ring at the producer index (not yet visible to
 * the SMMU: cmdq_publish() does that). */
static void cmdq_insert(uint64_t dw0, uint64_t dw1)
{
	uint32_t idx = smmu.cmdq_prod & SMMU_CMDQ_IDX_MASK;

	smmu.cmdq[idx * CMDQ_ENT_DWORDS] = dw0;
	smmu.cmdq[idx * CMDQ_ENT_DWORDS + 1] = dw1;
	smmu.cmdq_prod = (smmu.cmdq_prod + 1U) & SMMU_CMDQ_PTR_MASK;
}

/* Make the inserted commands visible, then hand them to the SMMU. */
static void cmdq_publish(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
	smmu_write32(smmu.base, SMMU_CMDQ_PROD, smmu.cmdq_prod);
}

/* Issue up to three commands followed by CMD_SYNC, and wait until the SMMU
 * has consumed the SYNC, i.e. completed every command before it.
 * Unused slots have dw0 == 0.  Returns HV_OK or HV_EIO. */
static hv_status_t cmdq_issue_sync(const uint64_t cmds[][2], uint32_t count)
{
	if (cmdq_wait_space(count + 1U) != 0)
		return HV_EIO;

	for (uint32_t i = 0; i < count; i++)
		cmdq_insert(cmds[i][0], cmds[i][1]);
	cmdq_insert(CMDQ_OP_SYNC | CMDQ_SYNC_0_CS_NONE, 0);
	cmdq_publish();

	for (uint32_t spin = 0; spin < SMMU_POLL_LIMIT; spin++) {
		uint32_t cons = smmu_read32(smmu.base, SMMU_CMDQ_CONS);

		if (cons & CMDQ_CONS_ERR_MASK)
			return HV_EIO;
		if ((cons & SMMU_CMDQ_PTR_MASK) == smmu.cmdq_prod)
			return HV_OK;
		__asm__ volatile("yield" ::: "memory");
	}
	return HV_EIO;
}

static uint64_t cmd_cfgi_ste(uint32_t sid)
{
	return CMDQ_OP_CFGI_STE | ((uint64_t)sid << CMDQ_CFGI_0_SID_SHIFT);
}

static uint64_t cmd_tlbi_s12_vmall(uint32_t vmid)
{
	return CMDQ_OP_TLBI_S12_VMALL |
	       ((uint64_t)(vmid & 0xffffU) << CMDQ_TLBI_0_VMID_SHIFT);
}

/* After an STE write: drop the cached STE and the TLB entries of the VMIDs
 * the stream used before (old_vmid) and uses now (new_vmid), then sync. */
static hv_status_t smmu_invalidate_ste(uint32_t sid, uint32_t old_vmid,
				       uint32_t new_vmid)
{
	uint64_t cmds[3][2];
	uint32_t n = 0;

	cmds[n][0] = cmd_cfgi_ste(sid);
	cmds[n++][1] = CMDQ_CFGI_1_LEAF;
	if (new_vmid != 0) {
		cmds[n][0] = cmd_tlbi_s12_vmall(new_vmid);
		cmds[n++][1] = 0;
	}
	if (old_vmid != 0 && old_vmid != new_vmid) {
		cmds[n][0] = cmd_tlbi_s12_vmall(old_vmid);
		cmds[n++][1] = 0;
	}
	return cmdq_issue_sync((const uint64_t (*)[2])cmds, n);
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/*
 * smmu_v3_init - initialize the SMMUv3: stream table, command queue, enable.
 *
 * Order (as Linux arm_smmu_device_reset): disable, program CR1 and the
 * stream table, program and enable the command and event queues,
 * invalidate cached configuration and TLBs, then enable translation.  All
 * STEs start as ABORT (fail-closed).  The event queue is enabled when the
 * SMMU supports at least SMMU_EVTQ_ENTRIES events (SMMU_IDR1.EVENTQS).
 *
 * CR1 makes the SMMU's own table and queue accesses write-back cacheable
 * and inner shareable, as Linux does.  This is valid for a coherent SMMU;
 * the CIX P1 firmware marks every SMMU node coherent
 * (data/common/cix-bios: Iort.aslc, EFI_ACPI_IORT_SMMUv3_FLAG_COHAC_OVERRIDE).
 *
 * @base:       Physical base address of SMMU MMIO registers
 * @nr_streams: Number of StreamIDs to support (<= SMMU_MAX_STREAMS)
 *
 * An SMMU without stage 2 (SMMU_IDR0.S2P = 0, e.g. QEMU before 9.2 or the
 * virt-9.1 machine) is still enabled with every stream aborted, so DMA stays
 * denied;
 * smmu_v3_set_ste_partition() then returns HV_ENOTSUP.
 *
 * Returns HV_OK, HV_EINVAL, HV_ENOTSUP (command queue too small) or HV_EIO
 * (the SMMU did not acknowledge or the queue reported an error).
 */
hv_status_t smmu_v3_init(uintptr_t base, uint32_t nr_streams)
{
	uint64_t strtab_base;
	uint32_t strtab_cfg;
	uint32_t cmdqs;
	uint32_t queues;
	uint64_t cmds[2][2];

	if (!base || nr_streams == 0 || nr_streams > SMMU_MAX_STREAMS)
		return HV_EINVAL;

	smmu.base = base;
	smmu.nr_streams = nr_streams;
	smmu.initialized = 0;

	smmu.s2 = (smmu_read32(base, SMMU_IDR0) & IDR0_S2P) != 0;
	cmdqs = (smmu_read32(base, SMMU_IDR1) & IDR1_CMDQS_MASK) >>
		IDR1_CMDQS_SHIFT;
	if (cmdqs < SMMU_CMDQ_LOG2)
		return HV_ENOTSUP;

	/* Disable SMMU (translation and queues) while configuring */
	smmu_write32(base, SMMU_CR0, 0);
	if (smmu_wait_cr0ack(base, 0) != 0)
		return HV_EIO;

	/* Initialize all STEs to ABORT */
	for (uint32_t i = 0; i < nr_streams; i++) {
		ste_write_abort(i);
		smmu.ste_vmid[i] = 0;
	}

	smmu_write32(base, SMMU_CR1, CR1_WB_ISH);

	/* Program stream table base: physical address + linear format */
	strtab_base = (uintptr_t)smmu.ste_table;
	smmu_write64(base, SMMU_STRTAB_BASE,
		     STRTAB_BASE_RA | (strtab_base & STRTAB_BASE_ADDR_MASK));

	/* Configure: linear table (FMT=0), LOG2SIZE = log2(nr_streams) */
	strtab_cfg = STRTAB_BASE_CFG_FMT_LINEAR;
	/* Simple log2 - nr_streams is a power of 2 in practice */
	{
		uint32_t sz = nr_streams, log2 = 0;
		while (sz > 1) {
			sz >>= 1;
			log2++;
		}
		strtab_cfg |= STRTAB_BASE_CFG_LOG2SIZE(log2);
	}
	smmu_write32(base, SMMU_STRTAB_BASE_CFG, strtab_cfg);

	/* Command queue: memory ring, empty, then enabled */
	for (uint32_t i = 0; i < SMMU_CMDQ_ENTRIES * CMDQ_ENT_DWORDS; i++)
		smmu.cmdq[i] = 0;
	__asm__ volatile("dsb sy" ::: "memory");
	smmu_write64(base, SMMU_CMDQ_BASE,
		     Q_BASE_RWA | ((uintptr_t)smmu.cmdq & Q_BASE_ADDR_MASK) |
			     (SMMU_CMDQ_LOG2 & Q_BASE_LOG2SIZE_MASK));
	smmu_write32(base, SMMU_CMDQ_PROD, 0);
	smmu_write32(base, SMMU_CMDQ_CONS, 0);
	smmu.cmdq_prod = 0;
	queues = CR0_CMDQ_ENABLE;

	if (((smmu_read32(base, SMMU_IDR1) & IDR1_EVTQS_MASK) >>
	     IDR1_EVTQS_SHIFT) >= SMMU_EVTQ_LOG2) {
		for (uint32_t i = 0; i < SMMU_EVTQ_ENTRIES * EVTQ_ENT_DWORDS;
		     i++)
			smmu.evtq[i] = 0;
		__asm__ volatile("dsb sy" ::: "memory");
		smmu_write64(base, SMMU_EVTQ_BASE,
			     Q_BASE_RWA |
				     ((uintptr_t)smmu.evtq & Q_BASE_ADDR_MASK) |
				     (SMMU_EVTQ_LOG2 & Q_BASE_LOG2SIZE_MASK));
		smmu_write32(base, SMMU_EVTQ_PROD, 0);
		smmu_write32(base, SMMU_EVTQ_CONS, 0);
		queues |= CR0_EVTQ_ENABLE;
	}

	smmu_write32(base, SMMU_CR0, queues);
	if (smmu_wait_cr0ack(base, queues) != 0)
		return HV_EIO;

	/* Nothing cached from before Haven may survive */
	cmds[0][0] = CMDQ_OP_CFGI_ALL;
	cmds[0][1] = CMDQ_CFGI_1_RANGE_ALL;
	cmds[1][0] = CMDQ_OP_TLBI_NSNH_ALL;
	cmds[1][1] = 0;
	if (cmdq_issue_sync((const uint64_t (*)[2])cmds, 2) != HV_OK)
		return HV_EIO;

	/* Enable SMMU translation */
	smmu_write32(base, SMMU_CR0, queues | CR0_SMMU_ENABLE);
	if (smmu_wait_cr0ack(base, queues | CR0_SMMU_ENABLE) != 0)
		return HV_EIO;

	smmu.initialized = 1;
	return HV_OK;
}

/*
 * smmu_v3_enable_event_irq - signal new events on the SMMU's wired event
 * interrupt (and global errors on its gerror interrupt).
 * Returns HV_OK, HV_ENOTSUP without an event queue, or HV_EIO.
 */
hv_status_t smmu_v3_enable_event_irq(void)
{
	uint32_t want = IRQ_CTRL_EVTQ_IRQEN | IRQ_CTRL_GERROR_IRQEN;

	if (!smmu.initialized)
		return HV_EINVAL;
	if (!(smmu_read32(smmu.base, SMMU_CR0ACK) & CR0_EVTQ_ENABLE))
		return HV_ENOTSUP;
	smmu_write32(smmu.base, SMMU_IRQ_CTRL, want);
	for (uint32_t spin = 0; spin < SMMU_POLL_LIMIT; spin++) {
		if (smmu_read32(smmu.base, SMMU_IRQ_CTRLACK) == want)
			return HV_OK;
	}
	return HV_EIO;
}

/*
 * smmu_v3_drain_events - hand every new event to fn, then move CONS past
 * them (acknowledging an overflow if the SMMU reported one).
 * Returns the number of events consumed.
 */
uint32_t smmu_v3_drain_events(void (*fn)(const uint64_t ev[EVTQ_ENT_DWORDS]))
{
	const uint32_t ptr = (SMMU_EVTQ_ENTRIES << 1) - 1U;
	uint32_t prod, cons, n = 0;

	if (!smmu.initialized)
		return 0;
	prod = smmu_read32(smmu.base, SMMU_EVTQ_PROD);
	cons = smmu_read32(smmu.base, SMMU_EVTQ_CONS) & ptr;
	__asm__ volatile("dsb sy" ::: "memory");
	while (cons != (prod & ptr)) {
		fn(&smmu.evtq[(cons & (SMMU_EVTQ_ENTRIES - 1U)) *
			      EVTQ_ENT_DWORDS]);
		cons = (cons + 1U) & ptr;
		n++;
	}
	smmu_write32(smmu.base, SMMU_EVTQ_CONS,
		     cons | (prod & Q_OVERFLOW_FLAG));
	return n;
}

/* Log the first few events and then running totals; called per event by
 * smmu_v3_service_irq(). */
static uint32_t smmu_events_seen;

static void smmu_log_event(const uint64_t ev[EVTQ_ENT_DWORDS])
{
	extern void hv_printk(const char *fmt, ...);
	uint32_t n = ++smmu_events_seen;

	if (n <= 4U) {
		hv_printk("HAVEN: SMMU event 0x%x SID %u addr 0x%lx %s\n",
			  (unsigned)(ev[0] & 0xffU), (unsigned)(ev[0] >> 32),
			  (unsigned long)ev[2],
			  ((ev[1] >> 35) & 1U) ? "read" : "write");
	} else if ((n & (n - 1U)) == 0U) {
		hv_printk("HAVEN: SMMU events so far: %u\n", (unsigned)n);
	}
}

/*
 * smmu_v3_service_irq - event interrupt handler: drain and log the events.
 */
void smmu_v3_service_irq(void)
{
	(void)smmu_v3_drain_events(smmu_log_event);
}

/*
 * smmu_v3_set_ste_partition - translate a StreamID through a stage-2 table.
 *
 * Writes an S2_TRANS STE so DMA from this StreamID is translated through
 * the given table.  Any DMA outside the table's mappings faults.
 *
 * @sid:   StreamID to configure
 * @vttbr: VTTBR_EL2-format value; bits [51:4] give the table base
 * @vmid:  VMID that tags this stream's TLB entries (non-zero)
 *
 * Returns HV_OK, HV_EINVAL, HV_ENOTSUP (no stage 2 in this SMMU), or HV_EIO
 * if the invalidation did not complete
 * (the STE in memory is already written; the caller must treat the stream
 * as unknown and revoke).
 */
hv_status_t smmu_v3_set_ste_partition(uint32_t sid, uint64_t vttbr,
				      uint32_t vmid)
{
	uint32_t old;

	if (!smmu.initialized || sid >= smmu.nr_streams || vmid == 0 ||
	    vmid > 0xffffU)
		return HV_EINVAL;
	if (!smmu.s2)
		return HV_ENOTSUP;

	old = smmu.ste_vmid[sid];
	ste_write_s2(sid, vttbr, vmid);
	smmu.ste_vmid[sid] = (uint16_t)vmid;
	return smmu_invalidate_ste(sid, old, vmid);
}

/*
 * smmu_v3_set_ste_abort - revert a StreamID to ABORT (deny all DMA).
 *
 * Used by hv_smmu_revoke_dma_access() after ownership revocation.
 * The abort is in force when this returns HV_OK.
 */
hv_status_t smmu_v3_set_ste_abort(uint32_t sid)
{
	uint32_t old;

	if (!smmu.initialized || sid >= smmu.nr_streams)
		return HV_EINVAL;

	old = smmu.ste_vmid[sid];
	ste_write_abort(sid);
	smmu.ste_vmid[sid] = 0;
	return smmu_invalidate_ste(sid, old, 0);
}

/*
 * smmu_v3_set_ste_bypass - configure a StreamID for pass-through.
 *
 * Not used in normal isolation policy; provided for early boot
 * or diagnostics where SMMU isolation is temporarily relaxed.
 */
hv_status_t smmu_v3_set_ste_bypass(uint32_t sid)
{
	uint32_t old;

	if (!smmu.initialized || sid >= smmu.nr_streams)
		return HV_EINVAL;

	old = smmu.ste_vmid[sid];
	ste_write_bypass(sid);
	smmu.ste_vmid[sid] = 0;
	return smmu_invalidate_ste(sid, old, 0);
}
