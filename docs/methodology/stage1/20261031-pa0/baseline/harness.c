/* P-A0 frozen baseline: existing HAVEN policy plus structural/range/version/hash
 * checks. Contains no comparison against E and no candidate logic. */
#include <stdio.h>
#include <string.h>

#include "haven/irq_ownership.h"
#include "haven/stage2.h"

struct art {
	unsigned version, version_rec;
	unsigned long long hash, hash_rec;
};

struct tcase {
	const char *id;
	struct art art;
	int has_region, owner;
	unsigned long long ipa, pa, size;
	unsigned long long d_off, d_len;
	int pre_owner;
	int has_irq, irq_cpu;
};

#define OK_ART {1, 1, 0xabc, 0xabc}
#define PG 0x1000ULL
static const struct tcase cases[] = {
	{"V1", OK_ART, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"V2", OK_ART, 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0},
	{"F1", OK_ART, 1, 1, 0xFFFFFFFFFFFFF000ULL, 0x90000, 0x2000, 0x20, 4, 0, 0, 0},
	{"F2", OK_ART, 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 2, 0, 0},
	{"F3", OK_ART, 1, 2, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0},
	{"F4", {2, 1, 0xdef, 0xabc}, 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0},
	{"F5", OK_ART, 1, 1, 0x10000, 0xA0000, PG, 0x20, 4, 0, 0, 0},
	/* F6: the portable API has no permission field; attrs 1 is the only MMIO form. */
	{"F6", OK_ART, 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0},
	{"F8", OK_ART, 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0},
	{"F9", OK_ART, 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 1, 1},
	{"F10", OK_ART, 1, 1, 0x20000, 0x20000, PG, 0x20, 4, 0, 0, 0},
};

static int structural(const struct tcase *c, const char **why)
{
	if (c->art.version != c->art.version_rec) {
		*why = "version";
		return -1;
	}
	if (c->art.hash != c->art.hash_rec) {
		*why = "hash";
		return -1;
	}
	if (c->has_region && c->ipa + c->size < c->ipa) {
		*why = "range-overflow";
		return -1;
	}
	if (c->has_region && c->d_off + c->d_len > c->size) {
		*why = "demand-outside-grant";
		return -1;
	}
	return 0;
}

static void run(const struct tcase *c)
{
	const char *why = "none";
	const char *verdict = "ACCEPT";
	int st = 0;

	hv_stage2_init();
	hv_irq_owner_init();

	if (structural(c, &why) != 0) {
		verdict = "REJECT";
		printf("%s %s checkpoint=structural:%s rc=na\n", c->id, verdict, why);
		return;
	}
	if (c->pre_owner) {
		struct hv_mem_region r = {0x50000, c->pa, c->size, 1};
		struct hv_partition_mem m = {(hv_u32)c->pre_owner, &r, 1};

		if (hv_stage2_map_partition(&m) != HV_OK) {
			printf("%s ERROR setup\n", c->id);
			return;
		}
	}
	if (c->has_region) {
		struct hv_mem_region r = {c->ipa, c->pa, c->size, 1};
		struct hv_partition_mem m = {(hv_u32)c->owner, &r, 1};

		st = hv_stage2_map_partition(&m);
		if (st != HV_OK) {
			verdict = "REJECT";
			why = "hv_stage2_map_partition";
		}
	}
	if (st == HV_OK && c->has_irq) {
		struct hv_irq_route rt = {40, (hv_u32)c->owner, (hv_u32)c->irq_cpu};

		st = hv_irq_assign(&rt);
		if (st != HV_OK) {
			verdict = "REJECT";
			why = "hv_irq_assign";
		}
	}
	printf("%s %s checkpoint=policy:%s rc=%d\n", c->id, verdict, why, st);
}

int main(void)
{
	unsigned i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
		run(&cases[i]);
	printf("F7 N/A checkpoint=non-scoring rc=na\n");
	return 0;
}
