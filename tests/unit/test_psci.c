/**
 * Unit tests for the minimal PSCI 1.0 handler (src/core/exc/psci.c).
 *
 * The cases follow what Linux v6.18 psci_probe() asks at boot, plus the
 * calls that stop the partition.
 *
 * @file tests/unit/test_psci.c
 */

#include <assert.h>
#include <haven/psci.h>
#include <stdio.h>

#define MPIDR_SELF 0x80000000ULL /* U bit clear, MT clear, Aff = 0 */

static hv_u64 call(hv_u64 x0, hv_u64 x1, hv_u64 x2, hv_psci_action_t *act)
{
	hv_u64 x[4] = {x0, x1, x2, 0};
	hv_u64 ret = 0xdeadULL;

	*act = hv_psci_call(x, MPIDR_SELF, &ret);
	return ret;
}

static int32_t code(hv_u64 r)
{
	assert(r == (hv_u64)(int64_t)(int32_t)r); /* sign-extended */
	return (int32_t)r;
}

static void test_boot_probe(void)
{
	hv_psci_action_t a;

	assert(call(HV_PSCI_VERSION, 0, 0, &a) == HV_PSCI_VERSION_1_0);
	assert(a == HV_PSCI_RESUME);
	assert(call(HV_PSCI_MIGRATE_INFO_TYPE, 0, 0, &a) == 2U);

	/* Linux keeps SMCCC v1.0 when SMCCC_VERSION is not supported. */
	assert(code(call(HV_PSCI_FEATURES, HV_SMCCC_VERSION, 0, &a)) ==
	       HV_PSCI_NOT_SUPPORTED);
	assert(code(call(HV_PSCI_FEATURES, HV_PSCI_CPU_SUSPEND_64, 0, &a)) ==
	       HV_PSCI_SUCCESS);
	assert(code(call(HV_PSCI_FEATURES, 0xC400000EU /* SYSTEM_SUSPEND */, 0,
			 &a)) == HV_PSCI_NOT_SUPPORTED);
	assert(code(call(HV_PSCI_FEATURES, 0xC4000012U /* SYSTEM_RESET2 */, 0,
			 &a)) == HV_PSCI_NOT_SUPPORTED);
	assert(code(call(HV_PSCI_FEATURES, 0xC4000015U /* SYSTEM_OFF2 */, 0,
			 &a)) == HV_PSCI_NOT_SUPPORTED);
	assert(code(call(HV_PSCI_FEATURES, HV_PSCI_FEATURES, 0, &a)) ==
	       HV_PSCI_SUCCESS);
	assert(a == HV_PSCI_RESUME);
	printf("[PASS] boot probe answers\n");
}

static void test_cpus(void)
{
	hv_psci_action_t a;

	assert(code(call(HV_PSCI_CPU_ON_64, 1, 0x40080000ULL, &a)) ==
	       HV_PSCI_DENIED);
	assert(a == HV_PSCI_RESUME);
	assert(code(call(HV_PSCI_CPU_SUSPEND_64, 0, 0x40080000ULL, &a)) ==
	       HV_PSCI_SUCCESS);
	assert(code(call(HV_PSCI_AFFINITY_INFO_64, MPIDR_SELF, 0, &a)) == 0);
	assert(code(call(HV_PSCI_AFFINITY_INFO_32, 0, 0, &a)) == 0);
	assert(code(call(HV_PSCI_AFFINITY_INFO_64, 1, 0, &a)) ==
	       HV_PSCI_INVALID_PARAMETERS);
	assert(code(call(HV_PSCI_AFFINITY_INFO_64, MPIDR_SELF, 1, &a)) ==
	       HV_PSCI_INVALID_PARAMETERS);
	printf("[PASS] CPU_ON denied, CPU_SUSPEND, AFFINITY_INFO\n");
}

static void test_stop(void)
{
	hv_psci_action_t a;

	(void)call(HV_PSCI_SYSTEM_OFF, 0, 0, &a);
	assert(a == HV_PSCI_STOP_OFF);
	(void)call(HV_PSCI_CPU_OFF, 0, 0, &a);
	assert(a == HV_PSCI_STOP_OFF);
	(void)call(HV_PSCI_SYSTEM_RESET, 0, 0, &a);
	assert(a == HV_PSCI_STOP_RESET);
	printf("[PASS] CPU_OFF, SYSTEM_OFF, SYSTEM_RESET stop the partition\n");
}

static void test_unknown(void)
{
	hv_psci_action_t a;

	assert(code(call(HV_SMCCC_VERSION, 0, 0, &a)) == HV_PSCI_NOT_SUPPORTED);
	assert(code(call(0xC4000012U, 0, 0, &a)) == HV_PSCI_NOT_SUPPORTED);
	assert(code(call(0x86000000U, 0, 0, &a)) == HV_PSCI_NOT_SUPPORTED);
	/* Upper bits of x0 set: not a valid function ID, even SYSTEM_OFF. */
	assert(code(call(0x100000000ULL | HV_PSCI_SYSTEM_OFF, 0, 0, &a)) ==
	       HV_PSCI_NOT_SUPPORTED);
	assert(a == HV_PSCI_RESUME);
	printf("[PASS] unknown and malformed IDs are NOT_SUPPORTED\n");
}

/* SMP0: a partition with vCPUs at MPIDR Aff 0 (self, on) and 1 (off). */
static struct hv_psci_vcpus two(void)
{
	struct hv_psci_vcpus v = {.count = 2U,
				  .mpidr = {0x80000000ULL, 0x80000001ULL},
				  .state = {HV_VCPU_ON, HV_VCPU_OFF}};
	return v;
}

static hv_u64 vcall(struct hv_psci_vcpus *v, hv_u64 self, hv_u64 x0, hv_u64 x1,
		    hv_u64 x2, hv_psci_action_t *act, hv_u32 *idx)
{
	hv_u64 x[4] = {x0, x1, x2, 0x1234};
	hv_u64 ret = 0xdeadULL;

	*act = hv_psci_call_vcpus(x, self, &ret, v, idx);
	return ret;
}

static void test_vcpus(void)
{
	struct hv_psci_vcpus v = two();
	hv_psci_action_t a;
	hv_u32 idx = 99U;

	/* AFFINITY_INFO follows the per-vCPU state: 0 ON, 1 OFF, 2 ON_PENDING */
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_AFFINITY_INFO_64, 1, 0, &a,
			  &idx)) == 1);
	/* CPU_ON for an unlisted CPU: INVALID_PARAMETERS, nothing started */
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_CPU_ON_64, 2,
			  0x40080000ULL, &a, &idx)) ==
	       HV_PSCI_INVALID_PARAMETERS);
	assert(a == HV_PSCI_RESUME && v.state[1] == HV_VCPU_OFF);
	/* CPU_ON for the vCPU that is on: ALREADY_ON */
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_CPU_ON_64, 0,
			  0x40080000ULL, &a, &idx)) == HV_PSCI_ALREADY_ON);
	assert(a == HV_PSCI_RESUME);
	/* CPU_ON for the vCPU that is off: start it (caller does the work) */
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_CPU_ON_64, 1,
			  0x40080000ULL, &a, &idx)) == HV_PSCI_SUCCESS);
	assert(a == HV_PSCI_START_VCPU && idx == 1U);
	assert(v.state[1] == HV_VCPU_ON_PENDING);
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_AFFINITY_INFO_64, 1, 0, &a,
			  &idx)) == 2);
	/* A second CPU_ON while it is starting: ON_PENDING */
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_CPU_ON_32, 1,
			  0x40080000ULL, &a, &idx)) == HV_PSCI_ON_PENDING);
	assert(a == HV_PSCI_RESUME);
	v.state[1] = HV_VCPU_ON; /* the caller confirms the start */
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_AFFINITY_INFO_64, 1, 0, &a,
			  &idx)) == 0);
	/* CPU_OFF from vCPU 1 stops only that vCPU */
	(void)vcall(&v, 0x80000001ULL, HV_PSCI_CPU_OFF, 0, 0, &a, &idx);
	assert(a == HV_PSCI_STOP_VCPU && idx == 1U &&
	       v.state[1] == HV_VCPU_OFF);
	/* SMP3: a vCPU that went off through CPU_OFF can be started again */
	idx = 99U;
	assert(code(vcall(&v, 0x80000000ULL, HV_PSCI_CPU_ON_64, 1,
			  0x40080000ULL, &a, &idx)) == HV_PSCI_SUCCESS);
	assert(a == HV_PSCI_START_VCPU && idx == 1U &&
	       v.state[1] == HV_VCPU_ON_PENDING);
	v.state[1] = HV_VCPU_ON;
	/* SYSTEM_OFF stops the partition, as before */
	(void)vcall(&v, 0x80000000ULL, HV_PSCI_SYSTEM_OFF, 0, 0, &a, &idx);
	assert(a == HV_PSCI_STOP_OFF);
	/* No vCPU table: the one-vCPU behaviour, CPU_ON denied */
	assert(code(vcall(NULL, 0x80000000ULL, HV_PSCI_CPU_ON_64, 1,
			  0x40080000ULL, &a, &idx)) == HV_PSCI_DENIED);
	printf("[PASS] two vCPUs: CPU_ON rules, AFFINITY_INFO states, CPU_OFF\n");
}

int main(void)
{
	test_boot_probe();
	test_cpus();
	test_stop();
	test_unknown();
	test_vcpus();
	printf("test_psci: all passed\n");
	return 0;
}
