/**
 * Unit tests for the emulated GICv3 distributor and redistributor
 * (src/core/irq/vgic.c), following what Linux v6.18's irq-gic-v3 driver
 * does at probe and when a driver enables its interrupt.
 *
 * @file tests/unit/test_vgic.c
 */

#include <assert.h>
#include <haven/vgic.h>
#include <stdio.h>

/* Partition owns SPI 33 and PPI 27 on CPU 0. */
static int hw_on[64];
static int hw_calls;

static int owned(hv_u32 cpu, hv_u32 id)
{
	return cpu == 0 && (id == 33 || id == 27);
}

static void hw_enable(hv_u32 cpu, hv_u32 id, int on)
{
	assert(owned(cpu, id)); /* never touches someone else's interrupt */
	hw_on[id] = on;
	hw_calls++;
}

static const struct hv_vgic_hooks hooks = {owned, hw_enable};

static hv_u64 rd(int (*f)(void *, hv_u64, hv_u32, int, hv_u64 *),
		 struct hv_vgic *v, hv_u64 off, hv_u32 size)
{
	hv_u64 x = 0xdeadbeefdeadbeefULL;

	assert(f(v, off, size, 0, &x) == 0);
	return x;
}

static void wr(int (*f)(void *, hv_u64, hv_u32, int, hv_u64 *),
	       struct hv_vgic *v, hv_u64 off, hv_u32 size, hv_u64 x)
{
	assert(f(v, off, size, 1, &x) == 0);
}

#define D hv_vgic_gicd_access
#define R hv_vgic_gicr_access
#define SGI 0x10000U

static void test_probe(struct hv_vgic *v)
{
	hv_u64 typer;

	/* Owned interrupts start physically disabled. */
	assert(hw_calls == 2 && hw_on[33] == 0 && hw_on[27] == 0);

	assert((rd(D, v, 0xFFE8, 4) & 0xF0) == 0x30); /* GICD_PIDR2: v3 */
	typer = rd(D, v, 0x0004, 4);
	assert(((typer & 0x1F) + 1) * 32 == 64); /* 64 IDs: 32 SPIs */
	assert((typer & (1U << 17)) == 0); /* no LPIs */
	assert((typer & (1U << 8)) == 0); /* no extended SPIs */
	assert(rd(D, v, 0x0000, 4) & (1U << 6)); /* DS = 1 */

	wr(D, v, 0x0000, 4, 0); /* disable, then wait for RWP */
	assert((rd(D, v, 0x0000, 4) & (1U << 31)) == 0);
	wr(D, v, 0x0000, 4, (1U << 4) | 3U); /* ARE_NS | Grp1A | Grp1 */
	assert(rd(D, v, 0x0000, 4) == ((1U << 4) | 3U | (1U << 6)));

	/* Redistributor: version, TYPER (64-bit and halves), WAKER */
	assert((rd(R, v, 0xFFE8, 4) & 0xF0) == 0x30);
	assert(rd(R, v, 0x0008, 8) == ((0ULL << 32) | (1U << 4)));
	assert(rd(R, v, 0x0008, 4) == (1U << 4));
	assert(rd(R, v, 0x000C, 4) == 0);
	assert(rd(R, v, 0x0014, 4) & (1U << 2)); /* asleep at reset */
	wr(R, v, 0x0014, 4, rd(R, v, 0x0014, 4) & ~(1U << 1));
	assert((rd(R, v, 0x0014, 4) & (1U << 2)) == 0); /* awake */
	assert((rd(R, v, 0x0000, 4) & (1U << 3)) == 0); /* GICR RWP */
	printf("[PASS] probe sequence\n");
}

static void test_dist_init(struct hv_vgic *v)
{
	/* gic_dist_init: groups, config, priorities, routing for SPIs */
	wr(D, v, 0x0084, 4, ~0U);
	assert(rd(D, v, 0x0084, 4) == ~0U);
	assert(rd(D, v, 0x0080, 4) == 0); /* SGI/PPI word: RAZ in GICD */
	wr(D, v, 0x0C08, 4, 0);
	wr(D, v, 0x0C0C, 4, 0x55555555U);
	assert(rd(D, v, 0x0C0C, 4) == 0x55555555U);
	wr(D, v, 0x0420, 4, 0xA0A0A0A0U); /* IDs 32..35 */
	assert(rd(D, v, 0x0420, 4) == 0xA0A0A0A0U);
	assert(rd(D, v, 0x0421, 1) == 0xA0); /* byte access */
	wr(D, v, 0x0400, 4, 0x80808080U); /* SGIs: not in GICD */
	assert(rd(D, v, 0x0400, 4) == 0);
	wr(D, v, 0x6108, 8, 0x0000000000000000ULL); /* IROUTER(33) */
	wr(D, v, 0x610C, 4, 0x12); /* upper half */
	assert(rd(D, v, 0x6108, 8) == 0x1200000000ULL);
	wr(D, v, 0x0184, 4, ~0U); /* ICENABLER1: all SPIs off */
	assert(rd(D, v, 0x0104, 4) == 0);
	assert(rd(D, v, 0x0200, 4) == 0 && rd(D, v, 0x0300, 4) == 0);
	assert(rd(D, v, 0x0008, 4) == 0 && rd(D, v, 0x000C, 4) == 0);
	printf("[PASS] distributor init writes\n");
}

static void test_enable(struct hv_vgic *v)
{
	int calls = hw_calls;

	/* UART driver enables SPI 33: physical enable follows */
	wr(D, v, 0x0104, 4, 1U << 1);
	assert(hw_on[33] == 1 && rd(D, v, 0x0104, 4) == (1U << 1));
	/* Enabling a SPI the partition does not own: ignored, no hw call */
	calls = hw_calls;
	wr(D, v, 0x0104, 4, 1U << 2);
	assert(hw_calls == calls && rd(D, v, 0x0104, 4) == (1U << 1));
	/* Enabling an enabled one again: no extra hw call */
	wr(D, v, 0x0104, 4, 1U << 1);
	assert(hw_calls == calls);
	/* Disable */
	wr(D, v, 0x0184, 4, 1U << 1);
	assert(hw_on[33] == 0 && rd(D, v, 0x0104, 4) == 0);

	/* Timer: PPI 27 through the redistributor's SGI frame */
	wr(R, v, SGI + 0x0100, 4, (1U << 27) | (1U << 26) | (1U << 1));
	assert(hw_on[27] == 1);
	assert(rd(R, v, SGI + 0x0100, 4) == ((1U << 27) | (1U << 1)));
	/* PPI configuration and priority; SGIs stay edge */
	wr(R, v, SGI + 0x0C04, 4, 0);
	wr(R, v, SGI + 0x0C00, 4, 0);
	assert(rd(R, v, SGI + 0x0C00, 4) == 0xAAAAAAAAU);
	wr(R, v, SGI + 0x0418, 4, 0xA0A0A0A0U); /* IDs 24..27 */
	assert(rd(R, v, SGI + 0x041B, 1) == 0xA0);
	wr(R, v, SGI + 0x0080, 4, ~0U);
	assert(rd(R, v, SGI + 0x0080, 4) == ~0U);
	assert(rd(R, v, SGI + 0x0084, 4) == 0); /* SPI word: not in GICR */
	printf("[PASS] enable follows ownership\n");
}

static void test_sgi(struct hv_vgic *v)
{
	/* SGI 1 enabled above (bit 1 of ISENABLER0) */
	assert(hv_vgic_sgi(v, (1ULL << 24) | 1U) == 1); /* self, aff0 0 */
	assert(hv_vgic_sgi(v, (1ULL << 24) | 2U) == -1); /* CPU aff0 1 */
	assert(hv_vgic_sgi(v, (1ULL << 24) | (1ULL << 40)) == -1); /* IRM */
	assert(hv_vgic_sgi(v, (1ULL << 24) | (1ULL << 16) | 1U) == -1);
	assert(hv_vgic_sgi(v, (5ULL << 24) | 1U) == -1); /* not enabled */
	assert(hv_vgic_for_cpu(0) == v && hv_vgic_for_cpu(1) == NULL);
	printf("[PASS] self-targeted SGIs only\n");
}

/* A vCPU whose MPIDR has every affinity level non-zero: Aff3 4, Aff2 1,
 * Aff1 2, Aff0 3. GICR_TYPER and SGI targeting must use all four. */
static void test_affinity(void)
{
	static struct hv_vgic v1;
	hv_u64 mpidr = (0x04ULL << 32) | 0x80000000ULL | (0x01U << 16) |
		       (0x02U << 8) | 0x03U;
	hv_u64 self = (1ULL << 24) | (1ULL << 3) | (0x02ULL << 16) |
		      (0x01ULL << 32) | (0x04ULL << 48);

	hv_vgic_init(&v1, 1, mpidr, &hooks);
	assert(rd(R, &v1, 0x0008, 8) == ((0x04010203ULL << 32) | (1U << 4)));
	assert(rd(R, &v1, 0x000C, 4) == 0x04010203U);
	wr(R, &v1, SGI + 0x0100, 4, 1U << 1);
	assert(hv_vgic_sgi(&v1, self) == 1);
	assert(hv_vgic_sgi(&v1, self & ~(0xFFULL << 48)) == -1); /* Aff3 0 */
	assert(hv_vgic_sgi(&v1, (self & ~0xFFFFULL) | 1U) == -1); /* Aff0 0 */
	assert(hv_vgic_for_cpu(1) == &v1);
	printf("[PASS] non-zero affinity in GICR_TYPER and SGI targets\n");
}

int main(void)
{
	static struct hv_vgic v;

	hv_vgic_init(&v, 0, 0x80000000ULL, &hooks);
	test_probe(&v);
	test_dist_init(&v);
	test_enable(&v);
	test_sgi(&v);
	test_affinity();
	printf("test_vgic: all passed\n");
	return 0;
}
