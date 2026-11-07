/**
 * Unit tests for trapped system register accesses
 * (src/core/exc/sysreg_trap.c): self-targeted SGIs are injected, other
 * registers read as zero, and only the first four are logged.
 *
 * @file tests/unit/test_sysreg_trap.c
 */

#include <assert.h>
#include <haven/mmio.h>
#include <haven/sysreg_trap.h>
#include <haven/vgic.h>
#include <stdio.h>

static int printk_calls;
static int injected = -1;
static int inject_calls;

void hv_printk(const char *fmt, ...)
{
	(void)fmt;
	printk_calls++;
}

hv_status_t hv_arch_inject_virtual_irq(uint32_t virq, uint32_t priority)
{
	(void)priority;
	injected = (int)virq;
	inject_calls++;
	return HV_OK;
}

static int posted_cpu = -1, posted_sgi = -1, post_calls;

void hv_arch_post_virtual_sgi(uint32_t cpu, uint32_t intid)
{
	posted_cpu = (int)cpu;
	posted_sgi = (int)intid;
	post_calls++;
}

static int owned_none(hv_u32 cpu, hv_u32 id)
{
	(void)cpu;
	(void)id;
	return 0;
}

static void hw_none(hv_u32 cpu, hv_u32 id, int on)
{
	(void)cpu;
	(void)id;
	(void)on;
	assert(0);
}

static const struct hv_vgic_hooks hooks = {owned_none, hw_none};

/* ESR_EL2 for EC 0x18 with the ISS fields of a system register access */
static hv_u64 esr(hv_u32 op0, hv_u32 op1, hv_u32 crn, hv_u32 crm, hv_u32 op2,
		  hv_u32 rt, int read)
{
	hv_u32 iss = (op0 << 20) | (op2 << 17) | (op1 << 14) | (crn << 10) |
		     (rt << 5) | (crm << 1) | (read ? 1U : 0U);

	return (0x18ULL << 26) | (1ULL << 25) | iss;
}

static hv_u64 frame[HV_FRAME_SIZE / 8U];

static void test_self_sgi(void)
{
	static struct hv_vgic v;
	hv_u64 en = 1U << 1;

	hv_vgic_init(&v, 0, 0x80000000ULL, &hooks);
	assert(hv_vgic_gicr_access(&v, 0x10000U + 0x0100U, 4, 1, &en) == 0);

	frame[HV_FRAME_ELR] = 0x1000;
	frame[5] = (1ULL << 24) | 1U; /* SGI 1 to self (aff0 0) */
	hv_handle_sysreg_trap(esr(3, 0, 12, 11, 5, 5, 0), frame, 0);
	assert(inject_calls == 1 && injected == 1);
	assert(frame[HV_FRAME_ELR] == 0x1004 && printk_calls == 0);

	frame[5] = (1ULL << 24) | 2U; /* aff0 1: another CPU, dropped */
	hv_handle_sysreg_trap(esr(3, 0, 12, 11, 5, 5, 0), frame, 0);
	assert(inject_calls == 1 && printk_calls == 0);
	printf("[PASS] self-targeted SGI injected, others dropped\n");
}

static void test_read_as_zero(void)
{
	frame[HV_FRAME_ELR] = 0x2000;
	frame[3] = 0x1234;
	hv_handle_sysreg_trap(esr(3, 0, 4, 6, 0, 3, 1), frame, 0); /* PMR */
	assert(frame[3] == 0 && frame[HV_FRAME_ELR] == 0x2004);
	assert(printk_calls == 1);

	frame[3] = 0x5678; /* a write is ignored, register kept */
	hv_handle_sysreg_trap(esr(3, 0, 4, 6, 0, 3, 0), frame, 0);
	assert(frame[3] == 0x5678 && frame[HV_FRAME_ELR] == 0x2008);
	printf("[PASS] unhandled registers read as zero, writes ignored\n");
}

static void test_log_cap(void)
{
	int i;

	/* two logged above; eight more accesses log only two more */
	for (i = 0; i < 8; i++)
		hv_handle_sysreg_trap(esr(3, 0, 4, 6, 0, 3, 1), frame, 0);
	assert(printk_calls == 4);
	printf("[PASS] log stops after four accesses\n");
}

/* SMP0: vCPU on CPU 0 (Aff0 0) sends SGI 2 to its peer on CPU 1 (Aff0 1). */
static void test_cross_cpu_sgi(void)
{
	static struct hv_vgic v0, v1;
	struct hv_vgic *peers[2] = {&v0, &v1};
	hv_u64 en = 1U << 2;

	hv_vgic_init(&v0, 0, 0x80000000ULL, &hooks);
	hv_vgic_init(&v1, 1, 0x80000001ULL, &hooks);
	hv_vgic_set_peers(peers, 2U);
	assert(hv_vgic_gicr_access(&v1, 0x10000U + 0x0100U, 4, 1, &en) == 0);
	assert(hv_vgic_gicr_access(&v0, 0x10000U + 0x0100U, 4, 1, &en) == 0);
	inject_calls = 0;
	frame[HV_FRAME_ELR] = 0x3000;
	frame[6] = (2ULL << 24) | 2U; /* SGI 2 to Aff0 1 */
	hv_handle_sysreg_trap(esr(3, 0, 12, 11, 5, 6, 0), frame, 0);
	assert(post_calls == 1 && posted_cpu == 1 && posted_sgi == 2);
	assert(inject_calls == 0 && frame[HV_FRAME_ELR] == 0x3004);
	frame[6] = (2ULL << 24) | 3U; /* to both: local injection and a post */
	hv_handle_sysreg_trap(esr(3, 0, 12, 11, 5, 6, 0), frame, 0);
	assert(post_calls == 2 && inject_calls == 1 && injected == 2);
	hv_vgic_set_peers(NULL, 0U);
	printf("[PASS] SGI for another vCPU posted to its CPU\n");
}

int main(void)
{
	test_self_sgi();
	test_read_as_zero();
	test_log_cap();
	test_cross_cpu_sgi();
	printf("test_sysreg_trap: all passed\n");
	return 0;
}
