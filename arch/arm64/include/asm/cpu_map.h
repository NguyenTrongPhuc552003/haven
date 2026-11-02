/* SPDX-License-Identifier: Apache-2.0 */
/*
 * arch/arm64/include/asm/cpu_map.h - logical CPU numbering (FINDINGS B3).
 *
 * Haven numbers CPUs by their position in a per-platform table of MPIDR
 * affinity values; entry 0 is the boot CPU.  Nothing derives a CPU index
 * from MPIDR bit fields: on CIX P1 every core has Aff0 = 0 and its core
 * number in Aff1 (vendor MADT, GET_MPID(core, 0)), with core 4 as the boot
 * CPU, while QEMU virt numbers CPUs in Aff0.
 *
 * secondary_entry (boot.S) parks a CPU whose MPIDR is not in the table, or
 * that claims entry 0, before it touches any per-CPU stack or mailbox.
 */
#ifndef HAVEN_ASM_CPU_MAP_H
#define HAVEN_ASM_CPU_MAP_H

/* Per-CPU stacks, mailboxes and handshake words (boot.S, smp.c). */
#define HV_MAX_CPUS 16
#define HV_STACK_SIZE 4096

/* MPIDR affinity fields: Aff3 [39:32], Aff2..Aff0 [23:0]. */
#define HV_MPIDR_AFF_MASK 0xff00ffffffUL

/* Secondary CPU states (hv_cpu_state[]), written by that CPU. */
#define HV_CPU_OFF 0U
#define HV_CPU_ENTERED 1U /* secondary_entry found its index */
#define HV_CPU_READY 2U /* per-CPU EL2 setup done, waiting for go */

/* Primary's answer (hv_cpu_go[]), written by the boot CPU. */
#define HV_CPU_WAIT 0U
#define HV_CPU_GO 1U
#define HV_CPU_ABORT 2U

#define HV_CPU_INVALID 0xffffffffU

#ifndef __ASSEMBLY__
#include <stdint.h>

/* Platform table: MPIDR affinity of each logical CPU, boot CPU first.
 * Every platform defines both (no weak default: see arch/arm64/cpu.c). */
extern const uint64_t hv_cpu_mpidr[];
extern const uint32_t hv_cpu_count;

/* Logical index of the calling CPU, or HV_CPU_INVALID. */
uint32_t hv_arch_cpu_id(void);
/* MPIDR affinity of logical CPU cpu, or HV_CPU_INVALID if out of range. */
uint64_t hv_arch_logical_mpidr(uint32_t cpu);

/* Primary: wait up to timeout_us for cpu to report ready, then tell it to
 * go (returns 1) or to park (returns 0).  A CPU that reports later sees
 * the abort and parks. */
int hv_arch_cpu_wait_ready(uint32_t cpu, uint64_t timeout_us);
/* Secondary: report ready and wait for the primary's answer; returns 1 to
 * go, 0 to park. */
int hv_arch_cpu_report_ready(uint32_t cpu);
#endif

#endif
