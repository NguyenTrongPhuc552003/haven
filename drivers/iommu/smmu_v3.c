/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/iommu/smmu_v3.c - ARM SMMUv3 stream table driver.
 *
 * Implements the hardware binding for Haven's DMA isolation policy.
 * The policy layer (src/core/dma/smmu.c) validates ownership and calls
 * these functions to write Stream Table Entries (STEs) in hardware.
 *
 * Linear Stream Table (STE format type 0):
 *   - One 64-byte STE per StreamID.
 *   - STE.CONFIG selects: ABORT (deny), BYPASS (passthrough), S2_TRANS
 * (isolate).
 *   - For S2_TRANS: STE.S2TTB points to the partition's stage-2 page table,
 *     STE.S2VMID matches the partition's VMID.
 *
 * Haven's isolation policy:
 *   - Unassigned StreamIDs → ABORT STE (fail-closed default).
 *   - Assigned StreamIDs → S2_TRANS STE using partition's VTTBR_EL2.
 *   - Revocation atomically replaces S2_TRANS STE with ABORT STE.
 *
 * References:
 *   ARM IHI 0070E - ARM System Memory Management Unit v3 Architecture
 */

#include <asm/smmu.h>
#include <haven/types.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Stream Table Entry (STE) layout - 64 bytes (8 × uint64_t)
 * Defined in ARM SMMUv3 spec Table 6-4.
 * ----------------------------------------------------------------------- */

#define SMMU_STE_DWORDS 8 /* 8 × 8 bytes = 64 bytes per STE */
#define SMMU_MAX_STREAMS \
	256 /* Linear table size (Haven conservative limit)   \
                              */

/* -----------------------------------------------------------------------
 * Driver state
 * ----------------------------------------------------------------------- */

static struct {
	uintptr_t base;
	uint64_t ste_table[SMMU_MAX_STREAMS * SMMU_STE_DWORDS]
		__attribute__((aligned(SMMU_MAX_STREAMS * 64)));
	uint32_t nr_streams;
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

/* Write an S2_TRANS STE for a StreamID: stage-2 translation using
 * the given partition's VTTBR (stage-2 page table base) and VMID. */
static void ste_write_s2(uint32_t sid, uint64_t vttbr, uint32_t vmid)
{
	uint64_t *ste = &smmu.ste_table[sid * SMMU_STE_DWORDS];

	/* Word 0: VALID + CONFIG_S2_TRANS */
	ste[0] = STE_0_VALID | STE_0_CONFIG_S2_TRANS;

	/* Word 1: reserved (no stage-1 context) */
	ste[1] = 0;

	/* Words 2-3: stage-2 attributes - VMID, IPA size, granule, AA64 flag.
   * STE2_S2_ENCODE encodes: VMID, T0SZ=25 (39-bit IPA), SL0=1 (L2 start),
   * IRGN=1 (WB), ORGN=1 (WB), SH=3 (IS), TG=0 (4KB), PS=2 (40-bit PA),
   * AA64=1. */
	ste[2] = STE2_S2_ENCODE(vmid);

	/* Word 3: stage-2 table base address (low 48 bits of VTTBR) */
	ste[3] = vttbr & 0x0000ffffffffffff;

	/* Words 4-7: reserved */
	for (int i = 4; i < SMMU_STE_DWORDS; i++)
		ste[i] = 0;

	__asm__ volatile("dsb sy" ::: "memory");
}

/* Invalidate STE in SMMU TLB after table write.
 * Issues CFGI_STE command via command queue. */
static void smmu_invalidate_ste(uint32_t sid)
{
	uint64_t cmd[2];
	uint32_t prod, cons;

	/* CMD_CFGI_STE: opcode=0x03, SID, LEAF=1 */
	cmd[0] = (0x03ULL) | ((uint64_t)sid << 32) | (1ULL << 12);
	cmd[1] = 0;

	/* Write command to queue (single-entry poll loop for simplicity) */
	prod = smmu_read32(smmu.base, SMMU_CMDQ_PROD) & 0x7fffffff;
	cons = smmu_read32(smmu.base, SMMU_CMDQ_CONS) & 0x7fffffff;
	(void)cons;

	/* Write the two dwords at prod slot (assumes queue is not full) */
	smmu_write64(smmu.base, SMMU_CMDQ_BASE + prod * 16, cmd[0]);
	smmu_write64(smmu.base, SMMU_CMDQ_BASE + prod * 16 + 8, cmd[1]);

	/* Advance producer */
	smmu_write32(smmu.base, SMMU_CMDQ_PROD, (prod + 1) & 0x7fffffff);

	/* Issue CMD_SYNC to wait for completion */
	cmd[0] = 0x46ULL; /* SYNC opcode */
	cmd[1] = 0;
	prod = smmu_read32(smmu.base, SMMU_CMDQ_PROD) & 0x7fffffff;
	smmu_write64(smmu.base, SMMU_CMDQ_BASE + prod * 16, cmd[0]);
	smmu_write64(smmu.base, SMMU_CMDQ_BASE + prod * 16 + 8, cmd[1]);
	smmu_write32(smmu.base, SMMU_CMDQ_PROD, (prod + 1) & 0x7fffffff);

	/* Wait for CONS to catch up */
	do {
		cons = smmu_read32(smmu.base, SMMU_CMDQ_CONS) & 0x7fffffff;
	} while (cons != (smmu_read32(smmu.base, SMMU_CMDQ_PROD) & 0x7fffffff));
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/*
 * smmu_v3_init - initialize the SMMUv3 and install the linear stream table.
 *
 * Configures SMMU_STRTAB_BASE with our static stream table, sets all STEs
 * to ABORT (fail-closed), then enables the SMMU.
 *
 * @base:       Physical base address of SMMU MMIO registers
 * @nr_streams: Number of StreamIDs to support (≤ SMMU_MAX_STREAMS)
 */
hv_status_t smmu_v3_init(uintptr_t base, uint32_t nr_streams)
{
	uint64_t strtab_base;
	uint32_t strtab_cfg;

	if (!base || nr_streams == 0 || nr_streams > SMMU_MAX_STREAMS)
		return HV_EINVAL;

	smmu.base = base;
	smmu.nr_streams = nr_streams;
	smmu.initialized = 0;

	/* Disable SMMU while configuring */
	smmu_write32(base, SMMU_CR0, 0);
	smmu_wait_cr0ack(base, 0);

	/* Initialize all STEs to ABORT */
	for (uint32_t i = 0; i < nr_streams; i++)
		ste_write_abort(i);

	/* Program stream table base: physical address + linear format */
	strtab_base = (uintptr_t)smmu.ste_table;
	smmu_write64(base, SMMU_STRTAB_BASE,
		     strtab_base & STRTAB_BASE_ADDR_MASK);

	/* Configure: linear table (FMT=0), LOG2SIZE = log2(nr_streams) */
	strtab_cfg = 0; /* FMT_LINEAR */
	/* Simple log2 - nr_streams is a power of 2 in practice */
	{
		uint32_t sz = nr_streams, log2 = 0;
		while (sz > 1) {
			sz >>= 1;
			log2++;
		}
		strtab_cfg |= log2;
	}
	smmu_write32(base, SMMU_STRTAB_BASE_CFG, strtab_cfg);

	/* Enable SMMU translation */
	smmu_write32(base, SMMU_CR0, CR0_SMMU_ENABLE);
	smmu_wait_cr0ack(base, CR0_SMMU_ENABLE);

	smmu.initialized = 1;
	return HV_OK;
}

/*
 * smmu_v3_set_ste_partition - assign a StreamID to a partition's stage-2.
 *
 * Writes an S2_TRANS STE so DMA from this StreamID is translated through
 * the partition's stage-2 page table.  Any DMA outside the mapped IPA
 * range triggers a stage-2 fault.
 *
 * @sid:  StreamID to configure
 * @vttbr: VTTBR_EL2 value for the partition's stage-2 table
 * @vmid: Partition VMID (must match VTTBR encoding)
 */
hv_status_t smmu_v3_set_ste_partition(uint32_t sid, uint64_t vttbr,
				      uint32_t vmid)
{
	if (!smmu.initialized || sid >= smmu.nr_streams)
		return HV_EINVAL;

	ste_write_s2(sid, vttbr, vmid);
	smmu_invalidate_ste(sid);
	return HV_OK;
}

/*
 * smmu_v3_set_ste_abort - revert a StreamID to ABORT (deny all DMA).
 *
 * Used by hv_smmu_revoke_dma_access() after ownership revocation.
 * The abort takes effect after the next CFGI_STE command completes.
 */
hv_status_t smmu_v3_set_ste_abort(uint32_t sid)
{
	if (!smmu.initialized || sid >= smmu.nr_streams)
		return HV_EINVAL;

	ste_write_abort(sid);
	smmu_invalidate_ste(sid);
	return HV_OK;
}

/*
 * smmu_v3_set_ste_bypass - configure a StreamID for pass-through.
 *
 * Not used in normal isolation policy; provided for early boot
 * or diagnostics where SMMU isolation is temporarily relaxed.
 */
hv_status_t smmu_v3_set_ste_bypass(uint32_t sid)
{
	if (!smmu.initialized || sid >= smmu.nr_streams)
		return HV_EINVAL;

	ste_write_bypass(sid);
	smmu_invalidate_ste(sid);
	return HV_OK;
}
