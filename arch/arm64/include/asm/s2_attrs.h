/* SPDX-License-Identifier: Apache-2.0 */
/* Translate portable HV_MEM_* bits into ARM64 stage-2 descriptor attributes.
 *
 * Kept as a pure function in its own header so host tests can check the
 * translation without compiling the rest of arch/arm64/mm.c (MM1/MM2).
 * Precondition: attrs passed hv_stage2_attrs_valid(), so HV_MEM_R is set and
 * HV_MEM_DEVICE never comes with HV_MEM_X. */

#ifndef HAVEN_ASM_S2_ATTRS_H
#define HAVEN_ASM_S2_ATTRS_H

#include <stdint.h>

#include "haven/stage2.h"
#include "page.h"

static inline uint64_t hv_s2_hw_attrs(uint64_t attrs)
{
	uint64_t hw;

	if ((attrs & HV_MEM_DEVICE) != 0U) {
		hw = (uint64_t)(S2_MEMATTR_DEVICE_nGnRE | S2_SH_NS);
	} else {
		hw = (uint64_t)(S2_MEMATTR_NORMAL_WB | S2_SH_IS);
	}

	hw |= ((attrs & HV_MEM_W) != 0U) ? (uint64_t)S2AP_RW :
					   (uint64_t)S2AP_RO;

	if ((attrs & HV_MEM_X) == 0U) {
		hw |= (uint64_t)S2_XN_NEVER;
	}

	return hw;
}

#endif
