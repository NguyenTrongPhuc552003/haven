/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Minimal PSCI 1.0 for a partition with one vCPU.
 *
 * Supported: PSCI_VERSION, PSCI_FEATURES, MIGRATE_INFO_TYPE,
 * AFFINITY_INFO (own vCPU only), CPU_SUSPEND (returns at once, which the
 * spec allows as an immediate wake-up), CPU_ON (DENIED for one vCPU; for
 * a multi-vCPU partition see hv_psci_call_vcpus),
 * CPU_OFF, SYSTEM_OFF and SYSTEM_RESET (the partition is stopped).
 * Everything else returns NOT_SUPPORTED, including SMCCC_VERSION, so a
 * Linux guest keeps SMCCC v1.0 and makes no further SMCCC calls.
 *
 * @file src/core/exc/psci.c
 */

#include <haven/psci.h>

static hv_u64 psci_ret(int32_t code)
{
	/* Sign-extend: callers read a negative code as a 32-bit int. */
	return (hv_u64)(int64_t)code;
}

static int psci_supported(hv_u32 fid)
{
	switch (fid) {
	case HV_PSCI_VERSION:
	case HV_PSCI_CPU_SUSPEND_32:
	case HV_PSCI_CPU_SUSPEND_64:
	case HV_PSCI_CPU_OFF:
	case HV_PSCI_CPU_ON_32:
	case HV_PSCI_CPU_ON_64:
	case HV_PSCI_AFFINITY_INFO_32:
	case HV_PSCI_AFFINITY_INFO_64:
	case HV_PSCI_MIGRATE_INFO_TYPE:
	case HV_PSCI_SYSTEM_OFF:
	case HV_PSCI_SYSTEM_RESET:
	case HV_PSCI_FEATURES:
		return 1;
	default:
		return 0;
	}
}

hv_psci_action_t hv_psci_call(const hv_u64 x[4], hv_u64 mpidr, hv_u64 *ret)
{
	hv_u32 fid = (hv_u32)x[0];
	hv_u64 target;

	*ret = psci_ret(HV_PSCI_NOT_SUPPORTED);

	/* An SMC64/HVC64 function ID has bit 30 set; the upper 32 bits of x0
	 * must be zero for any valid function ID (SMCCC). */
	if ((x[0] >> 32) != 0U)
		return HV_PSCI_RESUME;

	switch (fid) {
	case HV_PSCI_VERSION:
		*ret = HV_PSCI_VERSION_1_0;
		break;
	case HV_PSCI_FEATURES:
		/* CPU_SUSPEND feature flags 0: original power_state format,
		 * platform-coordinated mode only. 0 for the others too. */
		*ret = psci_supported((hv_u32)x[1]) ?
			       psci_ret(HV_PSCI_SUCCESS) :
			       psci_ret(HV_PSCI_NOT_SUPPORTED);
		break;
	case HV_PSCI_MIGRATE_INFO_TYPE:
		*ret = HV_PSCI_TOS_NOT_PRESENT_MP;
		break;
	case HV_PSCI_AFFINITY_INFO_32:
	case HV_PSCI_AFFINITY_INFO_64:
		target = (fid == HV_PSCI_AFFINITY_INFO_32) ? (hv_u32)x[1] :
							     x[1];
		/* Only level 0 is asked by Linux; one vCPU, which is ON. */
		if (x[2] == 0U && (target & HV_PSCI_MPIDR_AFF_MASK) ==
					  (mpidr & HV_PSCI_MPIDR_AFF_MASK))
			*ret = psci_ret(HV_PSCI_SUCCESS); /* 0 = ON */
		else
			*ret = psci_ret(HV_PSCI_INVALID_PARAMETERS);
		break;
	case HV_PSCI_CPU_SUSPEND_32:
	case HV_PSCI_CPU_SUSPEND_64:
		*ret = psci_ret(HV_PSCI_SUCCESS);
		break;
	case HV_PSCI_CPU_ON_32:
	case HV_PSCI_CPU_ON_64:
		*ret = psci_ret(HV_PSCI_DENIED);
		break;
	case HV_PSCI_CPU_OFF:
	case HV_PSCI_SYSTEM_OFF:
		return HV_PSCI_STOP_OFF;
	case HV_PSCI_SYSTEM_RESET:
		return HV_PSCI_STOP_RESET;
	default:
		break;
	}
	return HV_PSCI_RESUME;
}

/* Read-back (tools/readback/readback.py, v14) reads HAVEN's vCPU table
 * (part_a_vcpus) at these offsets, with the table size it reads here;
 * keep them in step. */
const hv_u32 hv_psci_max_vcpus = HV_PSCI_MAX_VCPUS;
_Static_assert(__builtin_offsetof(struct hv_psci_vcpus, count) == 0,
	       "readback.py reads hv_psci_vcpus.count at offset 0");
_Static_assert(__builtin_offsetof(struct hv_psci_vcpus, state) ==
		       8U + 8U * HV_PSCI_MAX_VCPUS,
	       "readback.py reads hv_psci_vcpus.state at 8 + 8 * max");

static int vcpu_index(const struct hv_psci_vcpus *v, hv_u64 mpidr)
{
	for (hv_u32 i = 0; i < v->count && i < HV_PSCI_MAX_VCPUS; i++)
		if ((v->mpidr[i] & HV_PSCI_MPIDR_AFF_MASK) ==
		    (mpidr & HV_PSCI_MPIDR_AFF_MASK))
			return (int)i;
	return -1;
}

hv_psci_action_t hv_psci_call_vcpus(const hv_u64 x[4], hv_u64 mpidr,
				    hv_u64 *ret, struct hv_psci_vcpus *v,
				    hv_u32 *index)
{
	hv_u32 fid = (hv_u32)x[0];
	hv_u64 target;
	int i;

	if (v == NULL || (x[0] >> 32) != 0U)
		return hv_psci_call(x, mpidr, ret);
	switch (fid) {
	case HV_PSCI_CPU_ON_32:
	case HV_PSCI_CPU_ON_64:
		target = (fid == HV_PSCI_CPU_ON_32) ? (hv_u32)x[1] : x[1];
		i = vcpu_index(v, target);
		if (i < 0) {
			*ret = psci_ret(HV_PSCI_INVALID_PARAMETERS);
		} else if (v->state[i] == HV_VCPU_ON) {
			*ret = psci_ret(HV_PSCI_ALREADY_ON);
		} else if (v->state[i] == HV_VCPU_ON_PENDING) {
			*ret = psci_ret(HV_PSCI_ON_PENDING);
		} else {
			v->state[i] = HV_VCPU_ON_PENDING;
			*index = (hv_u32)i;
			*ret = psci_ret(HV_PSCI_SUCCESS);
			return HV_PSCI_START_VCPU;
		}
		return HV_PSCI_RESUME;
	case HV_PSCI_AFFINITY_INFO_32:
	case HV_PSCI_AFFINITY_INFO_64:
		target = (fid == HV_PSCI_AFFINITY_INFO_32) ? (hv_u32)x[1] :
							     x[1];
		i = vcpu_index(v, target);
		*ret = (x[2] == 0U && i >= 0) ?
			       (hv_u64)v->state[i] :
			       psci_ret(HV_PSCI_INVALID_PARAMETERS);
		return HV_PSCI_RESUME;
	case HV_PSCI_CPU_OFF:
		i = vcpu_index(v, mpidr);
		if (v->count > 1U && i >= 0) {
			v->state[i] = HV_VCPU_OFF;
			*index = (hv_u32)i;
			return HV_PSCI_STOP_VCPU;
		}
		return HV_PSCI_STOP_OFF;
	default:
		return hv_psci_call(x, mpidr, ret);
	}
}

const char *hv_psci_name(hv_u32 fid)
{
	switch (fid) {
	case HV_PSCI_VERSION:
		return "PSCI_VERSION";
	case HV_PSCI_CPU_SUSPEND_32:
	case HV_PSCI_CPU_SUSPEND_64:
		return "CPU_SUSPEND";
	case HV_PSCI_CPU_OFF:
		return "CPU_OFF";
	case HV_PSCI_CPU_ON_32:
	case HV_PSCI_CPU_ON_64:
		return "CPU_ON";
	case HV_PSCI_AFFINITY_INFO_32:
	case HV_PSCI_AFFINITY_INFO_64:
		return "AFFINITY_INFO";
	case HV_PSCI_MIGRATE_INFO_TYPE:
		return "MIGRATE_INFO_TYPE";
	case HV_PSCI_SYSTEM_OFF:
		return "SYSTEM_OFF";
	case HV_PSCI_SYSTEM_RESET:
		return "SYSTEM_RESET";
	case HV_PSCI_FEATURES:
		return "PSCI_FEATURES";
	case HV_SMCCC_VERSION:
		return "SMCCC_VERSION";
	default:
		return "?";
	}
}
