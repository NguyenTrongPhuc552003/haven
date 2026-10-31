/* P-A0 step 4: candidate A checker (consistency relations), host model only.
 *
 * Independence rules followed:
 *   - Case inputs are copied verbatim from the frozen baseline harness
 *     (baseline/harness.c) plus the envelope and variant data in
 *     freeze/cases.json ("envelope_E", "envelope_variants"). The effective
 *     binding H is a modelled input taken from the frozen case text; it equals
 *     the grant G unless the frozen input says otherwise (F6).
 *   - This file contains no expected verdicts. Scoring is done separately by
 *     compare.py, which is the only place the frozen "expected" field is read.
 *
 * Relations (stages/RESEARCH_CHARTER.md section 3):
 *   R1 fits(D, E)      demanded register range uses only permitted offsets
 *   R2 within(G, E)    owner, IPA->PA association, extent, access, IRQ route
 *   R3 granule(G, E)   no forbidden register shares an enforcement granule
 *                      with the grant (enforcement cannot be finer than a page)
 *   R4 enforces(H, G)  effective access is no broader than the grant and E
 * Each rejection reports the first failing relation, so per-class detection
 * can be attributed. Addresses, IRQ 40 and registers are fictional.
 */
#include <stdio.h>
#include <string.h>

enum access { ACC_NONE = 0, ACC_RO = 1, ACC_RW = 3 };

struct env {
	unsigned long long ipa, pa, size, granule;
	int owner, cpu;
	enum access access;
	unsigned long long permitted_off; /* single permitted offset (0x20) */
	unsigned long long forbidden_off; /* 0 = none */
	int has_irq, irq_id, irq_owner, irq_cpu;
};

struct tcase {
	const char *id;
	char env_variant; /* 'E' base, 'r' E_ro, 's' E_sub, 'i' E_irq */
	/* grant G and demand D, verbatim from baseline/harness.c */
	int has_region, owner;
	unsigned long long ipa, pa, size;
	unsigned long long d_off, d_len;
	int has_irq, irq_cpu;
	int pre_owner; /* baseline F2 setup; exclusivity is existing policy's job */
	enum access g_access; /* grant access */
	/* effective binding H (modelled): access actually permitted */
	enum access h_access;
	/* structural fields, verbatim from the baseline (F1, F4) */
	unsigned version, version_rec;
	int overflow_input;
};

#define PG 0x1000ULL
/* G access: the frozen V2 operation is read/write32, so grants are RW,
 * except F6, whose frozen input makes E read-only and only H writable. */
static const struct tcase cases[] = {
	{"V1", 'E', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ACC_NONE, ACC_NONE, 1, 1, 0},
	{"V2", 'E', 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0, ACC_RW, ACC_RW, 1, 1, 0},
	{"F1", 'E', 1, 1, 0xFFFFFFFFFFFFF000ULL, 0x90000, 0x2000, 0x20, 4, 0, 0, 0, ACC_RW, ACC_RW, 1, 1, 1},
	{"F2", 'E', 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 2, ACC_RW, ACC_RW, 1, 1, 0},
	{"F3", 'E', 1, 2, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0, ACC_RW, ACC_RW, 1, 1, 0},
	{"F4", 'E', 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0, ACC_RW, ACC_RW, 2, 1, 0},
	{"F5", 'E', 1, 1, 0x10000, 0xA0000, PG, 0x20, 4, 0, 0, 0, ACC_RW, ACC_RW, 1, 1, 0},
	{"F6", 'r', 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0, ACC_RO, ACC_RW, 1, 1, 0},
	{"F8", 's', 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 0, 0, 0, ACC_RW, ACC_RW, 1, 1, 0},
	{"F9", 'i', 1, 1, 0x10000, 0x90000, PG, 0x20, 4, 1, 1, 0, ACC_RW, ACC_RW, 1, 1, 0},
	{"F10", 'E', 1, 1, 0x20000, 0x20000, PG, 0x20, 4, 0, 0, 0, ACC_RW, ACC_RW, 1, 1, 0},
};

/* freeze/cases.json "envelope_E" and "envelope_variants" */
static struct env envelope(char v)
{
	struct env e = {0x10000, 0x90000, 0x1000, 0x1000, 1, 0, ACC_RW,
			0x20, 0, 0, 0, 0, 0};
	if (v == 'r')
		e.access = ACC_RO;
	if (v == 's')
		e.forbidden_off = 0x40;
	if (v == 'i') {
		e.has_irq = 1;
		e.irq_id = 40;
		e.irq_owner = 1;
		e.irq_cpu = 0;
	}
	return e;
}

static const char *check(const struct tcase *c)
{
	struct env e = envelope(c->env_variant);
	unsigned long long g_end, e_end, gran_lo, gran_hi, fb;

	/* Structural layer kept identical to the baseline, so differences in
	 * outcome come only from R1-R4. */
	if (c->version != c->version_rec)
		return "structural:version";
	if (c->overflow_input)
		return "structural:range-overflow";
	if (!c->has_region && !c->has_irq)
		return NULL; /* V1: nothing requested beyond guest RAM */

	/* R1 fits(D, E) */
	if (c->d_len && (c->d_off != e.permitted_off || c->d_len > 4))
		return "R1:demand-not-permitted";

	/* R2 within(G, E) */
	if (c->owner != e.owner)
		return "R2:owner";
	if (c->ipa != e.ipa || c->pa != e.pa) {
		g_end = c->ipa + c->size;
		e_end = e.ipa + e.size;
		if (c->ipa >= e_end || g_end <= e.ipa)
			return "R2:outside-envelope";
		return "R2:association";
	}
	if (c->size > e.size)
		return "R2:extent";
	if ((c->g_access & ~e.access) != 0)
		return "R2:access";
	if (c->has_irq) {
		if (!e.has_irq)
			return "R2:irq-not-in-envelope";
		if (c->owner != e.irq_owner || c->irq_cpu != e.irq_cpu)
			return "R2:irq-route";
	}

	/* R3 granule(G, E): a forbidden register inside any enforcement granule
	 * the grant touches cannot be excluded by page-granular stage-2. */
	if (e.forbidden_off) {
		gran_lo = c->ipa & ~(e.granule - 1);
		gran_hi = (c->ipa + c->size + e.granule - 1) & ~(e.granule - 1);
		fb = e.ipa + e.forbidden_off;
		if (fb >= gran_lo && fb < gran_hi)
			return "R3:granularity";
	}

	/* R4 enforces(H, G) and H within E */
	if ((c->h_access & ~e.access) != 0)
		return "R4:effective-access";
	return NULL;
}

int main(void)
{
	unsigned i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		const char *why = check(&cases[i]);
		printf("%s %s checkpoint=%s\n", cases[i].id,
		       why ? "REJECT" : "ACCEPT", why ? why : "none");
	}
	printf("F7 N/A checkpoint=non-scoring\n");
	return 0;
}
