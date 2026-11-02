/* SPDX-License-Identifier: Apache-2.0 */
/* SM4: the stage-2 table address in a stream table entry must not carry the
 * VMID.  For every DMA VMID Haven hands out (0xC0 to 0xFF) and for VMID 1,
 * S2TTB computed from a VTTBR-format value must equal the table base. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arch/arm64/include/asm/smmu_ste.h"

int main(void)
{
	const uint64_t bases[] = {0x80010000ULL, 0x80418000ULL,
				  0x0000fffffffff000ULL};

	for (unsigned b = 0; b < sizeof(bases) / sizeof(bases[0]); b++) {
		for (uint64_t vmid = 0xC0; vmid <= 0xFF; vmid++) {
			uint64_t vttbr = bases[b] | (vmid << 48);

			assert(smmu_ste3_s2ttb(vttbr) == bases[b]);
		}
		assert(smmu_ste3_s2ttb(bases[b] | (1ULL << 48)) == bases[b]);
		/* CnP (bit 0) is not part of the address either. */
		assert(smmu_ste3_s2ttb(bases[b] | 1ULL) == bases[b]);
	}
	printf("[PASS] smmu_ste: S2TTB equals the table base for every VMID\n");
	return 0;
}
