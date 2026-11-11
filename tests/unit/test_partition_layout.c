/**
 * What partitions_launch() (src/core/partition.c) asks the policy modules
 * for, per platform layout (docs/methodology/stage2/20261109-d5-platform-
 * layout, Step 2). Built twice: with the qemu-virt layout, and with
 * HAVEN_PLATFORM_CIX_P1 and a Linux device tree IPA. Stage-2, SMMU, IRQ
 * ownership, budgets and printk are recording stubs.
 *
 * @file tests/unit/test_partition_layout.c
 */

#include <haven/budget_sched.h>
#include <haven/irq_ownership.h>
#include <haven/smmu.h>
#include <haven/stage2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void partitions_launch(void);

#define CHECK(c)                                                            \
	do {                                                                \
		if (!(c)) {                                                 \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
			exit(1);                                            \
		}                                                           \
	} while (0)

static struct hv_mem_region maps[2][4];
static hv_u32 map_parts[2], map_counts[2], nmaps, nadds;
static struct hv_irq_route routes[16];
static hv_u32 nroutes, nbudgets, nassign, nwindow;
static struct hv_budget budgets[2];
static hv_u64 win[4]; /* streamid, base, size, pa of the last window */
static char log_text[4096];

hv_status_t hv_stage2_map_partition(const struct hv_partition_mem *cfg)
{
	CHECK(nmaps < 2U && cfg->region_count <= 4U);
	map_parts[nmaps] = cfg->partition_id;
	map_counts[nmaps] = cfg->region_count;
	memcpy(maps[nmaps], cfg->regions,
	       cfg->region_count * sizeof(cfg->regions[0]));
	nmaps++;
	return HV_OK;
}

hv_status_t hv_stage2_add_regions(const struct hv_partition_mem *cfg)
{
	nadds += cfg->region_count;
	return HV_OK;
}

hv_status_t hv_stage2_unmap_partition(hv_u32 partition_id)
{
	(void)partition_id;
	return HV_OK;
}

hv_status_t hv_smmu_assign_streamid(hv_u32 partition, hv_u16 hw_streamid)
{
	CHECK(partition == 1U && hw_streamid == 8U);
	nassign++;
	return HV_OK;
}

hv_status_t hv_smmu_configure_dma_window(hv_u16 streamid, hv_u64 dma_base,
					 hv_u64 dma_size, hv_u64 phys_base,
					 hv_dma_access_t access)
{
	CHECK(access == HV_DMA_RW);
	win[0] = streamid;
	win[1] = dma_base;
	win[2] = dma_size;
	win[3] = phys_base;
	nwindow++;
	return HV_OK;
}

hv_status_t hv_smmu_revoke_dma_access(hv_u16 streamid)
{
	(void)streamid;
	return HV_OK;
}

hv_status_t hv_smmu_release_streamid(hv_u16 streamid)
{
	(void)streamid;
	return HV_OK;
}

hv_status_t hv_irq_assign(const struct hv_irq_route *route)
{
	CHECK(nroutes < 16U);
	routes[nroutes++] = *route;
	return HV_OK;
}

hv_status_t hv_budget_set(const struct hv_budget *budget)
{
	CHECK(nbudgets < 2U);
	budgets[nbudgets++] = *budget;
	return HV_OK;
}

void hv_printk(const char *fmt, ...)
{
	size_t n = strlen(log_text);
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(log_text + n, sizeof(log_text) - n, fmt, ap);
	va_end(ap);
}

void hv_panic(const char *msg)
{
	printf("FAIL: hv_panic(%s)\n", msg);
	exit(1);
}

static void region(hv_u32 m, hv_u32 i, hv_u64 ipa, hv_u64 pa, hv_u64 size,
		   hv_u64 attrs)
{
	CHECK(maps[m][i].ipa_base == ipa && maps[m][i].pa_base == pa);
	CHECK(maps[m][i].size == size && maps[m][i].attrs == attrs);
}

static void route(hv_u32 i, hv_u32 irq, hv_u32 part, hv_u32 cpu)
{
	CHECK(routes[i].irq_id == irq && routes[i].owner_partition_id == part);
	CHECK(routes[i].target_cpu == cpu);
}

int main(void)
{
	partitions_launch();
#if defined(HAVEN_PLATFORM_CIX_P1)
	/* D3 grant v0: RAM identity mapped at 4 GiB, UART2; no partition B */
	CHECK(nmaps == 1U && map_parts[0] == 1U && map_counts[0] == 2U);
	region(0, 0, 0x100000000ULL, 0x100000000ULL, 0x80000000ULL, HV_MEM_RWX);
	region(0, 1, 0x040D0000ULL, 0x040D0000ULL, 0x1000ULL, HV_MEM_DEVICE_RW);
	CHECK(nassign == 0U && nwindow == 0U && nadds == 0U);
	CHECK(nroutes == 2U);
	route(0, 330U, 1U, 0U);
	route(1, 27U, 1U, 0U); /* Linux guest: virtual timer on CPU 0 */
	CHECK(nbudgets == 1U && budgets[0].partition_id == 1U);
	CHECK(budgets[0].budget_ns == 8000000ULL &&
	      budgets[0].period_ns == 10000000ULL);
	CHECK(strstr(log_text, "partition B not started: no memory on this "
			       "platform") != NULL);
	CHECK(strstr(log_text, "(SPI 330 -> CPU 0)") != NULL);
	printf("test_partition_layout_cix: cix-p1 layout as D3 grant v0\n");
#else
	CHECK(nmaps == 2U && map_parts[0] == 1U && map_counts[0] == 3U);
	region(0, 0, 0x40000000ULL, 0x80800000ULL, 0x20000000ULL, HV_MEM_RWX);
	region(0, 1, 0x09000000ULL, 0x09000000ULL, 0x1000ULL, HV_MEM_DEVICE_RW);
	region(0, 2, 0x70000000ULL, 0xA4C00000ULL, 0x200000ULL, HV_MEM_R);
	CHECK(map_parts[1] == 2U && map_counts[1] == 1U);
	region(1, 0, 0x60000000ULL, 0xA0800000ULL, 0x4000000ULL, HV_MEM_RWX);
	CHECK(nassign == 1U && nwindow == 1U && nadds == 2U);
	CHECK(win[0] == 8U && win[1] == 0x5F000000ULL &&
	      win[2] == 0x100000ULL && win[3] == 0x9F800000ULL);
	CHECK(nroutes == 2U);
	route(0, 33U, 1U, 0U);
	route(1, 27U, 2U, 2U);
	CHECK(nbudgets == 2U);
	CHECK(strstr(log_text, "(SPI 33 -> CPU 0)") != NULL);
	printf("test_partition_layout: qemu-virt layout unchanged\n");
#endif
	return 0;
}
