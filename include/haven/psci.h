/* SPDX-License-Identifier: Apache-2.0 */
/**
 * PSCI calls from partitions (HVC or trapped SMC).
 *
 * HAVEN answers a minimal PSCI 1.0 for a partition with one vCPU.
 * Function IDs and return codes: Arm DEN0022 (PSCI). What Linux calls at
 * boot: drivers/firmware/psci/psci.c, psci_probe().
 *
 * @file include/haven/psci.h
 */

#ifndef HAVEN_PSCI_H
#define HAVEN_PSCI_H

#include <haven/types.h>

/* Function IDs (SMC32 / SMC64 where both exist) */
#define HV_PSCI_VERSION 0x84000000U
#define HV_PSCI_CPU_SUSPEND_32 0x84000001U
#define HV_PSCI_CPU_SUSPEND_64 0xC4000001U
#define HV_PSCI_CPU_OFF 0x84000002U
#define HV_PSCI_CPU_ON_32 0x84000003U
#define HV_PSCI_CPU_ON_64 0xC4000003U
#define HV_PSCI_AFFINITY_INFO_32 0x84000004U
#define HV_PSCI_AFFINITY_INFO_64 0xC4000004U
#define HV_PSCI_MIGRATE_INFO_TYPE 0x84000006U
#define HV_PSCI_SYSTEM_OFF 0x84000008U
#define HV_PSCI_SYSTEM_RESET 0x84000009U
#define HV_PSCI_FEATURES 0x8400000AU
#define HV_SMCCC_VERSION 0x80000000U

/* Return codes (x0, as a signed 32-bit value) */
#define HV_PSCI_SUCCESS 0
#define HV_PSCI_NOT_SUPPORTED (-1)
#define HV_PSCI_INVALID_PARAMETERS (-2)
#define HV_PSCI_DENIED (-3)
#define HV_PSCI_ALREADY_ON (-4)
#define HV_PSCI_ON_PENDING (-5)
#define HV_PSCI_INTERNAL_FAILURE (-6)

/* PSCI 1.0: major 1 in bits [31:16], minor 0 in bits [15:0] */
#define HV_PSCI_VERSION_1_0 0x00010000U

/* MIGRATE_INFO_TYPE 2: no Trusted OS that needs migrating */
#define HV_PSCI_TOS_NOT_PRESENT_MP 2U

/* MPIDR affinity fields Aff3 [39:32] and Aff2..Aff0 [23:0] */
#define HV_PSCI_MPIDR_AFF_MASK 0xFF00FFFFFFULL

typedef enum {
	HV_PSCI_RESUME = 0, /* write x0 and return to the partition */
	HV_PSCI_STOP_OFF = 1, /* CPU_OFF or SYSTEM_OFF: stop the partition */
	HV_PSCI_STOP_RESET = 2, /* SYSTEM_RESET: stop (no restart yet) */
	HV_PSCI_START_VCPU = 3, /* CPU_ON accepted: start vCPU *index */
	HV_PSCI_STOP_VCPU = 4, /* CPU_OFF of one vCPU of several */
} hv_psci_action_t;

/**
 * Answer one PSCI call.
 *
 * @param x       the caller's x0..x3 (function ID and arguments)
 * @param mpidr   the calling vCPU's MPIDR as the partition sees it
 * @param ret     value for the caller's x0 (only for HV_PSCI_RESUME)
 * @return        what to do with the partition
 *
 * Pure: no hardware access, so host tests can call it.
 */
hv_psci_action_t hv_psci_call(const hv_u64 x[4], hv_u64 mpidr, hv_u64 *ret);

/* Multi-vCPU partitions (docs/architecture/multi-vcpu.md): the calling
 * partition's vCPUs, each pinned to one CPU. AFFINITY_INFO returns these
 * state values directly (PSCI: 0 ON, 1 OFF, 2 ON_PENDING). */
#define HV_PSCI_MAX_VCPUS 4U
#define HV_VCPU_ON 0U
#define HV_VCPU_OFF 1U
#define HV_VCPU_ON_PENDING 2U

struct hv_psci_vcpus {
	hv_u32 count;
	hv_u64 mpidr[HV_PSCI_MAX_VCPUS]; /* as the guest sees it (VMPIDR) */
	hv_u32 state[HV_PSCI_MAX_VCPUS];
};

/** As hv_psci_call, for a partition whose vCPUs are listed in `v` (NULL:
 * one vCPU, as hv_psci_call). CPU_ON of a listed vCPU that is off marks
 * it ON_PENDING and returns HV_PSCI_START_VCPU with *index set (entry in
 * x[2], context in x[3]); the caller starts it and sets its state to ON,
 * or back to OFF with *ret = INTERNAL_FAILURE. CPU_OFF from one of
 * several vCPUs marks it OFF and returns HV_PSCI_STOP_VCPU. Pure. */
hv_psci_action_t hv_psci_call_vcpus(const hv_u64 x[4], hv_u64 mpidr,
				    hv_u64 *ret, struct hv_psci_vcpus *v,
				    hv_u32 *index);

/** Short name of a PSCI function ID for the log, or "?" */
const char *hv_psci_name(hv_u32 fid);

#endif /* HAVEN_PSCI_H */
