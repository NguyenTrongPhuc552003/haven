/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Trapped system register accesses from a partition (ESR_EL2 EC 0x18).
 *
 * @file include/haven/sysreg_trap.h
 */

#ifndef HAVEN_SYSREG_TRAP_H
#define HAVEN_SYSREG_TRAP_H

#include <haven/types.h>

/** Handle one trapped access on `cpu`. `frame` is the saved exception
 * frame (include/haven/mmio.h). A self-targeted ICC_SGI1R_EL1 write is
 * injected as a virtual SGI; anything else reads as zero and ignores
 * writes, and the first 4 such accesses are logged. The saved ELR is
 * advanced past the instruction. Arch-neutral, so host tests run it. */
void hv_handle_sysreg_trap(hv_u64 esr, hv_u64 *frame, hv_u32 cpu);

struct hv_vgic;

/** SGIs `ready` (bit = SGI) for vCPU t, sent from the vCPU on this CPU
 * (`here`, NULL if none): injected locally when t is that vCPU, else
 * posted to t's CPU (multi-vCPU partitions). */
void hv_vgic_deliver_sgis(const struct hv_vgic *here, const struct hv_vgic *t,
			  hv_u32 ready);

#endif /* HAVEN_SYSREG_TRAP_H */
