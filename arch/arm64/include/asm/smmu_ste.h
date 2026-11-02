/* SPDX-License-Identifier: Apache-2.0 */
/* SMMUv3 stream table entry fields that Haven computes from other values.
 * Pure functions, so host unit tests can check them. */

#ifndef HAVEN_ASM_SMMU_STE_H
#define HAVEN_ASM_SMMU_STE_H

#include <stdint.h>

/* VTTBR_EL2 format: BADDR in bits [47:1], VMID in bits [63:48]. */
#define SMMU_VTTBR_BADDR_MASK (0x0000ffffffffffffUL)

/* STE word 3, S2TTB: stage-2 table base, bits [51:4] (Linux
 * STRTAB_STE_3_S2TTB_MASK GENMASK_ULL(51, 4)). */
#define SMMU_STE3_S2TTB_MASK (0x000ffffffffffff0UL)

/* S2TTB from a VTTBR-format value.  The VMID must be cleared first: its low
 * bits [51:48] fall inside the S2TTB field and would corrupt the table
 * address (FINDINGS SM4).  The VMID goes in STE word 2 instead. */
static inline uint64_t smmu_ste3_s2ttb(uint64_t vttbr)
{
	return (vttbr & SMMU_VTTBR_BADDR_MASK) & SMMU_STE3_S2TTB_MASK;
}

#endif /* HAVEN_ASM_SMMU_STE_H */
