/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Host tests for hv_gicr_find_frames (D2, ledger row 25): redistributor
 * frames found by GICR_TYPER affinity, with the VLPIS stride and the Last
 * bit, in memory laid out like CIX P1 and like QEMU virt.
 *
 * @file tests/unit/test_gicr_frames.c
 */

#include <assert.h>
#include <haven/gicr_frames.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define K128 0x20000U
#define K256 0x40000U

static uint8_t region[13 * K256] __attribute__((aligned(4096)));

/* GICR_TYPER of the frame at `off`: affinity Aff3.Aff2.Aff1.Aff0 in
 * [63:32], VLPIS bit 1, Last bit 4. */
static void typer(uint32_t off, uint32_t aff, int vlpis, int last)
{
	uint32_t lo = (vlpis ? 1U << 1 : 0U) | (last ? 1U << 4 : 0U);

	memcpy(region + off + 0x8, &lo, 4);
	memcpy(region + off + 0xC, &aff, 4);
}

static uintptr_t at(uint32_t off)
{
	return (uintptr_t)region + off;
}

static void test_cix(void)
{
	/* logical CPU 0 is core 4 (the boot core), then cores 0-3, 5-11 */
	static const hv_u64 map[12] = {0x400, 0x000, 0x100, 0x200,
				       0x300, 0x500, 0x600, 0x700,
				       0x800, 0x900, 0xa00, 0xb00};
	uintptr_t f[12];

	memset(region, 0, sizeof(region));
	for (uint32_t c = 0; c < 12; c++)
		typer(c * K256, c << 8, 1, c == 11);
	assert(hv_gicr_find_frames(at(0), map, 12, f) == 12U);
	assert(f[0] == at(4 * K256)); /* boot core 4, not frame 0 */
	assert(f[1] == at(0));
	assert(f[5] == at(5 * K256) && f[11] == at(11 * K256));
	printf("[PASS] CIX-like: 256 KiB frames, boot core 4 found by affinity\n");
}

static void test_qemu(void)
{
	static const hv_u64 map[4] = {0, 1, 2, 3};
	uintptr_t f[4];

	memset(region, 0, sizeof(region));
	for (uint32_t c = 0; c < 4; c++)
		typer(c * K128, c, 0, c == 3);
	assert(hv_gicr_find_frames(at(0), map, 4, f) == 4U);
	for (uint32_t c = 0; c < 4; c++)
		assert(f[c] == at(c * K128));
	printf("[PASS] QEMU-like: 128 KiB frames, same as the old stride\n");
}

static void test_missing_and_last(void)
{
	/* CPU 2 (MPIDR 5) has no frame; a matching frame after Last is
	 * not used */
	static const hv_u64 map[3] = {0, 1, 5};
	uintptr_t f[3];

	memset(region, 0, sizeof(region));
	typer(0, 0, 0, 0);
	typer(K128, 1, 0, 1); /* Last */
	typer(2 * K128, 5, 0, 1); /* after Last: must not be read */
	assert(hv_gicr_find_frames(at(0), map, 3, f) == 2U);
	assert(f[0] == at(0) && f[1] == at(K128) && f[2] == 0U);
	/* Aff3 is compared too: MPIDR bits [39:32] go to TYPER [63:56] */
	{
		static const hv_u64 a3[1] = {0x0100000002ULL};
		uintptr_t g[1];

		memset(region, 0, sizeof(region));
		typer(0, 0x00000002U, 0, 0); /* Aff3 0: no match */
		typer(K128, 0x01000002U, 0, 1); /* Aff3 1, Aff0 2 */
		assert(hv_gicr_find_frames(at(0), a3, 1, g) == 1U);
		assert(g[0] == at(K128));
	}
	printf("[PASS] missing CPU not found; walk stops at Last; Aff3 used\n");
}

int main(void)
{
	test_cix();
	test_qemu();
	test_missing_and_last();
	printf("test_gicr_frames: all passed\n");
	return 0;
}
