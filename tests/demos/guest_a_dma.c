/* SPDX-License-Identifier: Apache-2.0 */
/*
 * tests/demos/guest_a_dma.c - DMA through the SMMU from partition A (G2).
 *
 * Runs at EL1 with the stage-1 MMU off, so every access is Device memory:
 * all accesses below are naturally aligned (built with -mstrict-align).
 *
 * If HAVEN enforces the partition's DMA grant (bit 0 of the entry argument),
 * this checks that HAVEN placed the function's BAR and that its config page
 * is read-only, then drives the PCIe function at 00:01.0 as a modern virtio
 * device
 * (virtio-rng-pci in QEMU) and asks it for random bytes twice:
 *   1. into a buffer inside the granted DMA window: must complete;
 *   2. into a buffer in partition A's RAM but outside the window: the SMMU
 *      must stop the device's write, so the buffer keeps its canary.  How
 *      the device reports the failed write is device-specific (QEMU's
 *      virtio model still completes the request), so the verdict is taken
 *      from memory, not from completion.
 * The device is polled; no interrupts are used.
 *
 * A virtio device uses the IOMMU only if it offers VIRTIO_F_ACCESS_PLATFORM
 * (virtio 1.2, 6; QEMU: iommu_platform=on); otherwise its DMA bypasses the
 * SMMU whatever the stream table says.  Without the feature the test does
 * not start the device.
 *
 * Addresses (QEMU virt, see src/platform/qemu-virt/memory.h): config page
 * of 00:01.0 at 0x40_1000_8000 (read-only), memory BAR at 0x1000_0000, DMA
 * window at IPA 0x5F00_0000 (1 MiB).  Virtio PCI layout: virtio
 * specification 1.2, section 4.1.4 (capabilities) and 2.7 (split queues).
 */

#include <stdint.h>

#define UART_DR 0x09000000UL
#define UART_FR 0x09000018UL
#define UART_FR_TXFF (1U << 5)

#define CFG_BASE 0x4010008000UL /* ECAM page of 00:01.0 */
#define BAR_BASE 0x10000000UL
#define WIN_BASE 0x5F000000UL /* granted DMA window, 1 MiB */
#define OUTSIDE_BUF 0x5E000000UL /* partition A RAM, outside the window */

#define ARG_DMA0 (1UL << 0)

/* virtio PCI capability types and common-configuration offsets */
#define VIRTIO_PCI_CAP_COMMON 1
#define VIRTIO_PCI_CAP_NOTIFY 2
#define CC_DFSELECT 0x00
#define CC_DF 0x04
#define CC_GFSELECT 0x08
#define CC_GF 0x0c
#define CC_STATUS 0x14
#define CC_Q_SELECT 0x16
#define CC_Q_SIZE 0x18
#define CC_Q_ENABLE 0x1c
#define CC_Q_NOFF 0x1e
#define CC_Q_DESCLO 0x20
#define CC_Q_DESCHI 0x24
#define CC_Q_AVAILLO 0x28
#define CC_Q_AVAILHI 0x2c
#define CC_Q_USEDLO 0x30
#define CC_Q_USEDHI 0x34

#define ST_ACK 1U
#define ST_DRIVER 2U
#define ST_DRIVER_OK 4U
#define ST_FEATURES_OK 8U

#define QSIZE 4U
#define DESC_F_WRITE 2U
#define POLL_LIMIT 20000000U

/* Split virtqueue in the window: descriptors, available ring, used ring,
 * then the inside buffer. */
#define Q_DESC (WIN_BASE + 0x0000UL)
#define Q_AVAIL (WIN_BASE + 0x1000UL)
#define Q_USED (WIN_BASE + 0x2000UL)
#define INSIDE_BUF (WIN_BASE + 0x3000UL)
#define BUF_LEN 64U
#define CANARY 0xa5a5a5a5a5a5a5a5UL

static inline void w8(uint64_t a, uint8_t v)
{
	*(volatile uint8_t *)a = v;
}
static inline void w16(uint64_t a, uint16_t v)
{
	*(volatile uint16_t *)a = v;
}
static inline void w32(uint64_t a, uint32_t v)
{
	*(volatile uint32_t *)a = v;
}
static inline void w64(uint64_t a, uint64_t v)
{
	*(volatile uint64_t *)a = v;
}
static inline uint8_t r8(uint64_t a)
{
	return *(volatile uint8_t *)a;
}
static inline uint16_t r16(uint64_t a)
{
	return *(volatile uint16_t *)a;
}
static inline uint32_t r32(uint64_t a)
{
	return *(volatile uint32_t *)a;
}
static inline void barrier(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
}

static void putc_(char c)
{
	while (r32(UART_FR) & UART_FR_TXFF)
		;
	w32(UART_DR, (uint32_t)(unsigned char)c);
}

static void puts_(const char *s)
{
	while (*s)
		putc_(*s++);
}

static void puthex(uint64_t v)
{
	puts_("0x");
	for (int i = 60; i >= 0; i -= 4) {
		unsigned d = (unsigned)(v >> i) & 0xfU;
		if (v >> i || i == 0)
			putc_((char)(d < 10 ? '0' + d : 'a' + d - 10));
	}
}

/* Config space: byte access through aligned 32-bit reads. */
static uint8_t cfg8(uint32_t off)
{
	return (uint8_t)(r32(CFG_BASE + (off & ~3U)) >> ((off & 3U) * 8));
}

static uint32_t cfg32(uint32_t off)
{
	return r32(CFG_BASE + off);
}

/* Haven places the memory BAR and enables the function; the config page is
 * read-only here.  Check that the BAR the virtio capabilities use is at
 * BAR_BASE and that memory decoding and bus mastering are on.
 * Returns 0, or -1 if not. */
static int check_bar(uint32_t bar)
{
	uint32_t off = 0x10U + 4U * bar;
	uint32_t lo = cfg32(off);

	if ((lo & ~0xfU) != (uint32_t)BAR_BASE)
		return -1;
	if ((lo & 0x6U) == 0x4U && cfg32(off + 4U) != 0U)
		return -1;
	return (cfg32(0x04) & 0x6U) == 0x6U ? 0 : -1;
}

/* Wait until used->idx reaches want. Returns 1 if it did. */
static int wait_used(uint16_t want)
{
	for (uint32_t i = 0; i < POLL_LIMIT; i++) {
		if (r16(Q_USED + 2) == want)
			return 1;
	}
	return 0;
}

static void fill(uint64_t buf)
{
	for (uint32_t i = 0; i < BUF_LEN; i += 8)
		w64(buf + i, CANARY);
}

/* Bytes of buf that no longer hold the canary, in 8-byte units. */
static uint32_t changed(uint64_t buf)
{
	uint32_t n = 0;

	for (uint32_t i = 0; i < BUF_LEN; i += 8)
		n += *(volatile uint64_t *)(buf + i) != CANARY ? 8U : 0U;
	return n;
}

static void request(uint16_t slot, uint64_t buf)
{
	uint64_t d = Q_DESC + 16UL * slot;

	w64(d, buf);
	w32(d + 8, BUF_LEN);
	w16(d + 12, DESC_F_WRITE);
	w16(d + 14, 0);
	w16(Q_AVAIL + 4 + 2UL * slot, slot);
	barrier();
	w16(Q_AVAIL + 2, (uint16_t)(slot + 1U));
	barrier();
}

static void dma_test(uint64_t arg)
{
	uint64_t common = 0, notify = 0;
	uint32_t notify_mult = 0, bar = 6, hi, n;
	int done;
	uint8_t cap;
	uint16_t noff;

	if (!(arg & ARG_DMA0)) {
		puts_("PARTITION_A: no DMA device granted\r\n");
		return;
	}
	if ((cfg32(0) & 0xffffU) != 0x1af4U) {
		puts_("PARTITION_A: DMA device absent\r\n");
		return;
	}

	/* Find the virtio common and notify capabilities. */
	cap = ((cfg32(0x04) >> 16) & 0x10U) ? (cfg8(0x34) & 0xfcU) : 0;
	while (cap) {
		if (cfg8(cap) == 0x09U) { /* vendor-specific: virtio */
			uint8_t type = cfg8(cap + 3U);
			uint32_t off = cfg32(cap + 8U);

			if (type == VIRTIO_PCI_CAP_COMMON ||
			    type == VIRTIO_PCI_CAP_NOTIFY) {
				bar = cfg8(cap + 4U);
				if (type == VIRTIO_PCI_CAP_COMMON) {
					common = BAR_BASE + off;
				} else {
					notify = BAR_BASE + off;
					notify_mult = cfg32(cap + 16U);
				}
			}
		}
		cap = cfg8(cap + 1U) & 0xfcU;
	}
	if (!common || !notify || bar > 5 || check_bar(bar) != 0) {
		puts_("PARTITION_A: DMA device not usable\r\n");
		return;
	}
	/* The config page is read-only: this write must not land. */
	w32(CFG_BASE + 0x10U + 4U * bar, 0x20000000U);
	if (check_bar(bar) != 0) {
		puts_("PARTITION_A: config write landed\r\n");
		return;
	}
	puts_("PARTITION_A: config page read-only\r\n");

	/* Device initialisation (virtio 1.2, 3.1.1). */
	w8(common + CC_STATUS, 0);
	while (r8(common + CC_STATUS) != 0)
		;
	w8(common + CC_STATUS, ST_ACK | ST_DRIVER);
	w32(common + CC_DFSELECT, 1);
	hi = r32(common + CC_DF);
	if (!(hi & 1U)) { /* VIRTIO_F_VERSION_1 (bit 32) */
		puts_("PARTITION_A: DMA device is not virtio 1\r\n");
		return;
	}
	if (!(hi & 2U)) { /* VIRTIO_F_ACCESS_PLATFORM (bit 33) */
		/* Its DMA would bypass the SMMU and reach physical memory
		 * directly: do not start it. */
		puts_("PARTITION_A: DMA device bypasses the IOMMU "
		      "(no VIRTIO_F_ACCESS_PLATFORM), not started\r\n");
		w8(common + CC_STATUS, 0x80U); /* FAILED */
		return;
	}
	w32(common + CC_GFSELECT, 0);
	w32(common + CC_GF, 0);
	w32(common + CC_GFSELECT, 1);
	w32(common + CC_GF, hi & 3U);
	w8(common + CC_STATUS, ST_ACK | ST_DRIVER | ST_FEATURES_OK);
	if (!(r8(common + CC_STATUS) & ST_FEATURES_OK)) {
		puts_("PARTITION_A: DMA device refused features\r\n");
		return;
	}

	/* Queue 0, in the DMA window. */
	for (uint64_t a = WIN_BASE; a < INSIDE_BUF + BUF_LEN; a += 8)
		w64(a, 0);
	w16(common + CC_Q_SELECT, 0);
	w16(common + CC_Q_SIZE, QSIZE);
	w32(common + CC_Q_DESCLO, (uint32_t)Q_DESC);
	w32(common + CC_Q_DESCHI, 0);
	w32(common + CC_Q_AVAILLO, (uint32_t)Q_AVAIL);
	w32(common + CC_Q_AVAILHI, 0);
	w32(common + CC_Q_USEDLO, (uint32_t)Q_USED);
	w32(common + CC_Q_USEDHI, 0);
	noff = r16(common + CC_Q_NOFF);
	w16(common + CC_Q_ENABLE, 1);
	w8(common + CC_STATUS,
	   ST_ACK | ST_DRIVER | ST_FEATURES_OK | ST_DRIVER_OK);
	notify += (uint64_t)noff * notify_mult;

	/* 1. Buffer inside the window: the device's bytes must arrive. */
	fill(INSIDE_BUF);
	request(0, INSIDE_BUF);
	w16(notify, 0);
	done = wait_used(1);
	n = changed(INSIDE_BUF);
	if (n != 0U) {
		puts_("PARTITION_A: DMA inside window OK, bytes written ");
		puthex(n);
		puts_("\r\n");
	} else {
		puts_("PARTITION_A: DMA inside window FAILED, completed ");
		puthex((uint64_t)done);
		puts_("\r\n");
	}

	/* 2. Buffer outside the window: the SMMU must stop the write. */
	fill(OUTSIDE_BUF);
	request(1, OUTSIDE_BUF);
	w16(notify, 0);
	done = wait_used(2);
	n = changed(OUTSIDE_BUF);
	if (n != 0U) {
		puts_("PARTITION_A: DMA outside window NOT blocked, bytes "
		      "written ");
		puthex(n);
		puts_("\r\n");
	} else {
		puts_("PARTITION_A: DMA outside window blocked, buffer "
		      "unchanged, completed ");
		puthex((uint64_t)done);
		puts_("\r\n");
	}
}

void guest_a_dma_test(uint64_t arg)
{
	dma_test(arg);
	puts_("PARTITION_A: DMA test done\r\n");
}
