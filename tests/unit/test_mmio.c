/**
 * Unit tests for MMIO emulation (src/core/exc/mmio.c): syndrome decoding,
 * frame register access, load extension and region dispatch.
 *
 * @file tests/unit/test_mmio.c
 */

#include <assert.h>
#include <haven/mmio.h>
#include <stdio.h>
#include <string.h>

/* ISS for a data abort: ISV | SAS | SSE | SRT | SF | WnR, EC 0x24, IL 1 */
static hv_u64 esr(unsigned sas, int sse, unsigned srt, int sf, int wnr)
{
	return (0x24ULL << 26) | (1ULL << 25) | (1ULL << 24) |
	       ((hv_u64)sas << 22) | ((hv_u64)sse << 21) | ((hv_u64)srt << 16) |
	       ((hv_u64)sf << 15) | ((hv_u64)wnr << 6) | 0x6U;
}

static void test_decode(void)
{
	hv_mmio_access_t a;

	/* Observed in L0/L1: GICD_PIDR2 read by Linux */
	assert(hv_mmio_decode(0x93880006ULL, &a) == 0);
	assert(a.size == 4 && a.reg == 8 && !a.write && !a.sign_extend &&
	       !a.reg64);

	assert(hv_mmio_decode(esr(3, 0, 30, 1, 1), &a) == 0);
	assert(a.size == 8 && a.reg == 30 && a.write && a.reg64);

	/* ISV clear: not decodable */
	assert(hv_mmio_decode(esr(2, 0, 1, 0, 0) & ~(1ULL << 24), &a) == -1);
	printf("[PASS] decode\n");
}

static void test_frame(void)
{
	hv_u64 f[HV_FRAME_SIZE / 8];

	memset(f, 0, sizeof(f));
	for (unsigned n = 0; n <= 30; n++)
		hv_frame_set(f, n, 0x1000U + n);
	for (unsigned n = 0; n <= 30; n++)
		assert(hv_frame_get(f, n) == 0x1000U + n);
	assert(f[19] == 0x1000U + 29 && f[20] == 0x1000U + 30); /* x29, x30 */
	assert(f[24] == 0x1000U + 19 && f[33] == 0x1000U + 28); /* x19..x28 */
	assert(f[HV_FRAME_ELR] == 0 && f[HV_FRAME_SPSR] == 0 && f[23] == 0);
	hv_frame_set(f, 31, 0xdead);
	assert(hv_frame_get(f, 31) == 0); /* XZR */
	printf("[PASS] frame slots, XZR\n");
}

static void test_extend(void)
{
	hv_mmio_access_t a = {1, 0, 0, 1, 1}; /* ldrsb xN */

	assert(hv_mmio_load_value(&a, 0x1F0ULL) == 0xFFFFFFFFFFFFFFF0ULL);
	a.reg64 = 0; /* ldrsb wN */
	assert(hv_mmio_load_value(&a, 0x80ULL) == 0xFFFFFF80ULL);
	a.sign_extend = 0; /* ldrb wN */
	assert(hv_mmio_load_value(&a, 0x1F0ULL) == 0xF0ULL);
	a.size = 4; /* ldr wN */
	assert(hv_mmio_load_value(&a, 0x123456789ULL) == 0x23456789ULL);
	a.size = 8;
	a.reg64 = 1; /* ldr xN */
	assert(hv_mmio_load_value(&a, ~0ULL) == ~0ULL);
	a.size = 2;
	assert(hv_mmio_store_value(&a, 0xABCD1234ULL) == 0x1234ULL);
	printf("[PASS] load extension, store truncation\n");
}

static hv_u64 last_off, last_val;
static int last_write;

static int model(void *ctx, hv_u64 off, hv_u32 size, int write, hv_u64 *v)
{
	(void)ctx;
	(void)size;
	last_off = off;
	last_write = write;
	if (write)
		last_val = *v;
	else
		*v = 0xCAFEF00DULL;
	return off < 0x100 ? 0 : -1;
}

static void test_dispatch(void)
{
	hv_u64 f[HV_FRAME_SIZE / 8];
	hv_mmio_access_t ld = {4, 21, 0, 0, 0}; /* ldr w21 */
	hv_mmio_access_t st = {4, 25, 1, 0, 0}; /* str w25 */

	memset(f, 0, sizeof(f));
	hv_mmio_reset();
	assert(hv_mmio_register(0, 0x8000000, 0x10000, model, NULL) == HV_OK);
	assert(hv_mmio_register(0, 0x8008000, 0x1000, model, NULL) ==
	       HV_EINVAL);
	assert(hv_mmio_register(0, 0x9000000, 0, model, NULL) == HV_EINVAL);

	hv_frame_set(f, 21, 0xFFFFFFFFFFFFFFFFULL);
	assert(hv_mmio_emulate(0, 0x8000010, &ld, f) == 0);
	assert(last_off == 0x10 && !last_write);
	assert(hv_frame_get(f, 21) == 0xCAFEF00DULL);

	hv_frame_set(f, 25, 0x1122334455667788ULL);
	assert(hv_mmio_emulate(0, 0x8000020, &st, f) == 0);
	assert(last_write && last_val == 0x55667788ULL);

	assert(hv_mmio_emulate(1, 0x8000010, &ld, f) == -1); /* other CPU */
	assert(hv_mmio_emulate(0, 0x7FFFFFC, &ld, f) == -1); /* outside */
	assert(hv_mmio_emulate(0, 0x800FFFE, &ld, f) == -1); /* straddles */
	assert(hv_mmio_emulate(0, 0x8000200, &ld, f) == -1); /* model refuses */
	printf("[PASS] dispatch\n");
}

int main(void)
{
	test_decode();
	test_frame();
	test_extend();
	test_dispatch();
	printf("test_mmio: all passed\n");
	return 0;
}
