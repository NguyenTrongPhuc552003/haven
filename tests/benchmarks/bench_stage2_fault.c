/* Required for clock_gettime / CLOCK_MONOTONIC under strict C11 */
#define _POSIX_C_SOURCE 200809L

/**
 * Stage-2 containment check latency benchmark.
 *
 * Measures the per-call overhead of hv_stage2_partition_contains_pa() -
 * the spatial containment policy path that fires on every potential
 * cross-partition access attempt. This is the primary stage-2 enforcement
 * hot path and its latency bounds the worst-case hypervisor overhead for
 * all memory-isolation enforcement operations.
 *
 * Cases benchmarked:
 *   1. hot path   - PA clearly inside the partition window (cache-warm)
 *   2. cold path  - PA clearly outside any partition window (miss path)
 *   3. low boundary  - PA at the start of the partition window
 *   4. high boundary - PA at the last valid byte of the partition window
 *
 * Methodology:
 *   - 100 000 iterations per case.
 *   - 1 000 warm-up iterations (discarded) before each case.
 *   - Timing via clock_gettime(CLOCK_MONOTONIC).
 *   - Reported values: min / mean / max (nanoseconds).
 *
 * Output:
 *   Human-readable table on stdout.
 *   JSON results written to build/benchmarks/stage2-fault.json.
 *
 * @file tests/benchmarks/bench_stage2_fault.c
 */

#include <haven/stage2.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define WARMUP_ITERS 1000
#define BENCH_ITERS 100000
#define NUM_CASES 4
#define OUT_PATH "build/benchmarks/stage2-fault.json"

/* Partition used for all benchmark cases. */
#define BENCH_PARTITION_ID 2U

/* Memory window: 64 MiB at 0x40000000. */
#define WINDOW_BASE 0x40000000ULL
#define WINDOW_SIZE 0x04000000ULL /* 64 MiB */

/* -----------------------------------------------------------------------
 * Timing primitive
 * ----------------------------------------------------------------------- */
static long long hv_now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

/* -----------------------------------------------------------------------
 * Result record
 * ----------------------------------------------------------------------- */
typedef struct {
	const char *name;
	long long min_ns;
	long long mean_ns;
	long long max_ns;
	long long iters;
} bench_result_t;

/* -----------------------------------------------------------------------
 * One-shot setup: initialize stage-2 and map the benchmark partition.
 * ----------------------------------------------------------------------- */
static void bench_setup(void)
{
	static struct hv_mem_region reg = {
		.ipa_base = WINDOW_BASE,
		.pa_base = WINDOW_BASE,
		.size = WINDOW_SIZE,
		.attrs = 0,
	};
	static struct hv_partition_mem cfg = {
		.partition_id = BENCH_PARTITION_ID,
		.regions = &reg,
		.region_count = 1,
	};
	hv_stage2_init();
	hv_stage2_map_partition(&cfg);
}

/* -----------------------------------------------------------------------
 * Generic timing loop over a fixed PA/size pair.
 * ----------------------------------------------------------------------- */
static bench_result_t run_case(const char *name, hv_u32 partition_id, hv_u64 pa,
			       hv_u64 size)
{
	bench_result_t r;
	memset(&r, 0, sizeof(r));
	r.name = name;
	r.iters = BENCH_ITERS;
	r.min_ns = 0x7fffffffffffffffLL;
	r.max_ns = 0;

	/* Warm-up - not timed. */
	for (int i = 0; i < WARMUP_ITERS; i++)
		hv_stage2_partition_contains_pa(partition_id, pa, size);

	/* Measured iterations. */
	long long total_ns = 0;
	for (int i = 0; i < BENCH_ITERS; i++) {
		long long t0 = hv_now_ns();
		(void)hv_stage2_partition_contains_pa(partition_id, pa, size);
		long long t1 = hv_now_ns();

		long long delta = t1 - t0;
		if (delta < 0)
			delta = 0;
		if (delta < r.min_ns)
			r.min_ns = delta;
		if (delta > r.max_ns)
			r.max_ns = delta;
		total_ns += delta;
	}

	r.mean_ns = total_ns / BENCH_ITERS;
	return r;
}

/* -----------------------------------------------------------------------
 * Print human-readable table.
 * ----------------------------------------------------------------------- */
static void print_table(bench_result_t *results, int n)
{
	printf("\n%-35s  %8s  %8s  %8s  %s\n", "benchmark", "min(ns)",
	       "mean(ns)", "max(ns)", "iters");
	printf("%-35s  %8s  %8s  %8s  %s\n",
	       "-----------------------------------", "--------", "--------",
	       "--------", "----------");
	for (int i = 0; i < n; i++) {
		printf("%-35s  %8lld  %8lld  %8lld  %lld\n", results[i].name,
		       results[i].min_ns, results[i].mean_ns, results[i].max_ns,
		       results[i].iters);
	}
	printf("\n");
}

/* -----------------------------------------------------------------------
 * Write JSON output.
 * ----------------------------------------------------------------------- */
static void write_json(bench_result_t *results, int n, const char *path)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "[bench] warning: cannot write %s\n", path);
		return;
	}

	fprintf(f, "{\n");
	fprintf(f, "  \"description\": \"Haven stage-2 containment check "
		   "latency benchmark\",\n");
	fprintf(f, "  \"iterations_per_bench\": %d,\n", BENCH_ITERS);
	fprintf(f, "  \"warmup_iters\": %d,\n", WARMUP_ITERS);
	fprintf(f, "  \"results\": [\n");

	for (int i = 0; i < n; i++) {
		fprintf(f,
			"    {\n"
			"      \"name\": \"%s\",\n"
			"      \"min_ns\": %lld,\n"
			"      \"mean_ns\": %lld,\n"
			"      \"max_ns\": %lld,\n"
			"      \"iters\": %lld\n"
			"    }%s\n",
			results[i].name, results[i].min_ns, results[i].mean_ns,
			results[i].max_ns, results[i].iters,
			(i < n - 1) ? "," : "");
	}

	fprintf(f, "  ]\n}\n");
	fclose(f);
	printf("[bench] results written to %s\n", path);
}

/* ===================================================================== */
/* main                                                                    */
/* ===================================================================== */
int main(void)
{
	printf("=== Haven Stage-2 Fault / Containment Check Latency Benchmark ===\n");
	printf("    iterations per case: %d  warmup: %d\n\n", BENCH_ITERS,
	       WARMUP_ITERS);

	bench_setup();

	bench_result_t results[NUM_CASES];
	int idx = 0;

	/*
   * Case 1 - hot path: PA is well inside the partition window.
   * This is the most common case (every intra-partition access).
   */
	results[idx++] = run_case("stage2_contains_hot_path",
				  BENCH_PARTITION_ID,
				  WINDOW_BASE + 0x01000000ULL, /* mid-window */
				  0x1000ULL);

	/*
   * Case 2 - cold path: PA is entirely outside any partition window.
   * Models an attempted cross-partition access; the check must reject it.
   */
	results[idx++] = run_case("stage2_contains_cold_path",
				  BENCH_PARTITION_ID,
				  0x80000000ULL, /* outside any mapped window */
				  0x1000ULL);

	/*
   * Case 3 - low boundary: PA starts at the first byte of the window.
   * Tests the lower-bound edge of the range comparison logic.
   */
	results[idx++] = run_case("stage2_contains_low_boundary",
				  BENCH_PARTITION_ID,
				  WINDOW_BASE, /* first byte */
				  0x1000ULL);

	/*
   * Case 4 - high boundary: PA is the last page of the window.
   * Tests the upper-bound edge of the range comparison logic.
   */
	results[idx++] =
		run_case("stage2_contains_high_boundary", BENCH_PARTITION_ID,
			 WINDOW_BASE + WINDOW_SIZE - 0x1000ULL, 0x1000ULL);

	print_table(results, NUM_CASES);
	write_json(results, NUM_CASES, OUT_PATH);

	return 0;
}
