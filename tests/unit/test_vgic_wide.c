/**
 * Unit test for the emulated GICv3 built with 352 interrupt IDs, the
 * cix-p1 width (docs/methodology/stage2/20261109-cix-p1-platform): the
 * board's UART2 is INTID 330 (SPI 298), above the default 64 IDs.
 * Built with -DHAVEN_VGIC_NR_IRQS=352U (tests/CMakeLists.txt,
 * scripts/dev/test.sh); tests/unit/test_vgic.c keeps the 64-ID build.
 *
 * @file tests/unit/test_vgic_wide.c
 */

#include <assert.h>
#include <haven/vgic.h>
#include <stdio.h>

_Static_assert(HV_VGIC_NR_IRQS == 352U, "build with HAVEN_VGIC_NR_IRQS=352U");

#define D hv_vgic_gicd_access
#define UART2 330U

static int hw_on[HV_VGIC_NR_IRQS];
static int hw_calls;

static int owned(hv_u32 cpu, hv_u32 id)
{
	return cpu == 0 && (id == UART2 || id == 27U);
}

static void hw_enable(hv_u32 cpu, hv_u32 id, int on)
{
	assert(owned(cpu, id));
	hw_on[id] = on;
	hw_calls++;
}

static const struct hv_vgic_hooks hooks = {owned, hw_enable};

static hv_u64 rd(struct hv_vgic *v, hv_u64 off, hv_u32 size)
{
	hv_u64 x = 0xdeadbeefdeadbeefULL;

	assert(D(v, off, size, 0, &x) == 0);
	return x;
}

static void wr(struct hv_vgic *v, hv_u64 off, hv_u32 size, hv_u64 x)
{
	assert(D(v, off, size, 1, &x) == 0);
}

int main(void)
{
	static struct hv_vgic v;
	hv_u64 typer;

	hv_vgic_init(&v, 0, 0x400ULL, &hooks);
	assert(hw_calls == 2 && hw_on[UART2] == 0); /* starts disabled */

	typer = rd(&v, 0x0004, 4);
	assert((typer & 0x1F) == 10U); /* ITLinesNumber 10: 352 IDs */
	printf("[PASS] GICD_TYPER.ITLinesNumber = 10 (352 IDs)\n");

	/* Linux enables SPI 298 = INTID 330: GICD_ISENABLER10, bit 10 */
	wr(&v, 0x0100 + 4U * 10U, 4, 1U << 10);
	assert(hw_on[UART2] == 1 && hw_calls == 3);
	assert(rd(&v, 0x0100 + 4U * 10U, 4) == (1U << 10));
	/* INTID 329 is not the partition's: write ignored, no hardware call */
	wr(&v, 0x0100 + 4U * 10U, 4, 1U << 9);
	assert(rd(&v, 0x0100 + 4U * 10U, 4) == (1U << 10) && hw_calls == 3);
	wr(&v, 0x0180 + 4U * 10U, 4, 1U << 10); /* ICENABLER10 */
	assert(hw_on[UART2] == 0 && rd(&v, 0x0100 + 4U * 10U, 4) == 0U);
	printf("[PASS] INTID 330 enable and disable reach the hardware hook\n");

	/* Routing, priority, configuration of INTID 330 are stored */
	wr(&v, 0x6000 + 8U * UART2, 8, 0x400ULL);
	assert(rd(&v, 0x6000 + 8U * UART2, 8) == 0x400ULL);
	wr(&v, 0x0400 + 328U, 4, 0xA0A0A0A0U); /* IPRIORITYR, IDs 328..331 */
	assert(rd(&v, 0x0400 + 328U, 4) == 0xA0A0A0A0U);
	wr(&v, 0x0C00 + 4U * (UART2 / 16U), 4, 2U << (2U * (UART2 % 16U)));
	assert(rd(&v, 0x0C00 + 4U * (UART2 / 16U), 4) ==
	       (2U << (2U * (UART2 % 16U))));
	printf("[PASS] INTID 330 IROUTER, priority and configuration stored\n");

	/* INTID 352 and above do not exist: read as zero, writes ignored */
	wr(&v, 0x0100 + 4U * 11U, 4, ~0ULL);
	assert(rd(&v, 0x0100 + 4U * 11U, 4) == 0U);
	wr(&v, 0x6000 + 8U * 352U, 8, 0x400ULL);
	assert(rd(&v, 0x6000 + 8U * 352U, 8) == 0U);
	wr(&v, 0x0400 + 352U, 4, 0xA0A0A0A0U);
	assert(rd(&v, 0x0400 + 352U, 4) == 0U);
	printf("[PASS] INTIDs 352 and above read as zero\n");

	printf("sizeof(struct hv_vgic) = %u\n",
	       (unsigned)sizeof(struct hv_vgic));
	printf("test_vgic_wide: all passed\n");
	return 0;
}
