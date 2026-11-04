/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Minimal PSCI 1.0 for a partition with one vCPU.
 *
 * Supported: PSCI_VERSION, PSCI_FEATURES, MIGRATE_INFO_TYPE,
 * AFFINITY_INFO (own vCPU only), CPU_SUSPEND (returns at once, which the
 * spec allows as an immediate wake-up), CPU_ON (always DENIED: one vCPU),
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
