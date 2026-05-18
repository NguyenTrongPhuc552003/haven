/* Required for clock_gettime / CLOCK_MONOTONIC under strict C11 */
#define _POSIX_C_SOURCE 200809L

/**
 * SMMU DMA policy enforcement latency benchmark.
 *
 * Measures the per-call overhead of SMMU access-control operations that
 * appear on every DMA transaction the hypervisor intercepts. High overhead
 * here directly taxes I/O-intensive partitions.
 *
 * Cases benchmarked:
 *   1. streamid_lookup_hot  — hv_smmu_check_dma_access on a cache-warm
 *                             StreamID inside its configured window (common
 *                             case for every permitted DMA transaction).
 *   2. window_boundary_low  — DMA address at the first byte of the window.
 *   3. window_boundary_high — DMA address at the last valid byte of the
 *                             window.
 *   4. access_denied        — DMA address outside the window (must reject).
 *   5. streamid_alloc       — hv_smmu_allocate_streamid overhead (cold path,
 *                             called once per device attach).
 *
 * Methodology:
 *   - 100 000 iterations per case.
 *   - 1 000 warm-up iterations (discarded) before each case.
 *   - Timing via clock_gettime(CLOCK_MONOTONIC).
 *   - Reported values: min / mean / max (nanoseconds).
 *   - streamid_alloc re-inits the SMMU state before each batch so there
 *     is always a free slot to allocate.
 *
 * Output:
 *   Human-readable table on stdout.
 *   JSON results written to build/benchmarks/smmu-policy.json.
 *
 * @file tests/benchmarks/bench_smmu_policy.c
 */

#include <haven/smmu.h>
#include <haven/stage2.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define WARMUP_ITERS 1000
#define BENCH_ITERS  100000
#define NUM_CASES    5
#define OUT_PATH     "build/benchmarks/smmu-policy.json"

/* Partition used for all SMMU benchmark cases. */
#define BENCH_PARTITION_ID 5U

/* DMA window: 16 MiB at IOVA 0x50000000, backed by PA 0x50000000. */
#define DMA_IOVA_BASE  0x50000000ULL
#define DMA_PHYS_BASE  0x50000000ULL
#define DMA_WINDOW_SZ  0x01000000ULL  /* 16 MiB */

/* The single StreamID allocated during setup, used by most cases. */
static hv_u16 g_sid = 0;

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
    long long   min_ns;
    long long   mean_ns;
    long long   max_ns;
    long long   iters;
} bench_result_t;

/* -----------------------------------------------------------------------
 * Common setup: init SMMU + stage-2, map partition, allocate one StreamID,
 * configure its DMA window.
 * ----------------------------------------------------------------------- */
static void bench_setup(void)
{
    static struct hv_mem_region reg = {
        .ipa_base = DMA_PHYS_BASE,
        .pa_base  = DMA_PHYS_BASE,
        .size     = DMA_WINDOW_SZ,
        .attrs    = 0,
    };
    static struct hv_partition_mem cfg = {
        .partition_id = BENCH_PARTITION_ID,
        .regions      = &reg,
        .region_count = 1,
    };

    hv_smmu_init();
    hv_stage2_init();
    hv_stage2_map_partition(&cfg);
    hv_smmu_allocate_streamid(BENCH_PARTITION_ID, &g_sid);
    hv_smmu_configure_dma_window(g_sid,
                                 DMA_IOVA_BASE,
                                 DMA_WINDOW_SZ,
                                 DMA_PHYS_BASE,
                                 HV_DMA_RW);
}

/* -----------------------------------------------------------------------
 * Generic timing loop for hv_smmu_check_dma_access.
 * ----------------------------------------------------------------------- */
static bench_result_t run_check_case(const char *name,
                                     hv_u64 dma_addr, hv_u64 size,
                                     hv_dma_access_t access)
{
    bench_result_t r;
    memset(&r, 0, sizeof(r));
    r.name   = name;
    r.iters  = BENCH_ITERS;
    r.min_ns = 0x7fffffffffffffffLL;
    r.max_ns = 0;

    /* Warm-up. */
    for (int i = 0; i < WARMUP_ITERS; i++)
        hv_smmu_check_dma_access(g_sid, dma_addr, size, access);

    /* Measured iterations. */
    long long total_ns = 0;
    for (int i = 0; i < BENCH_ITERS; i++) {
        long long t0 = hv_now_ns();
        (void)hv_smmu_check_dma_access(g_sid, dma_addr, size, access);
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
 * Timing loop for hv_smmu_allocate_streamid.
 * We re-init the SMMU before each batch so there is always a free slot.
 * ----------------------------------------------------------------------- */
static bench_result_t run_alloc_case(void)
{
    bench_result_t r;
    memset(&r, 0, sizeof(r));
    r.name   = "smmu_allocate_streamid";
    r.iters  = BENCH_ITERS;
    r.min_ns = 0x7fffffffffffffffLL;
    r.max_ns = 0;

    /* Warm-up: re-init each time so alloc always succeeds. */
    for (int i = 0; i < WARMUP_ITERS; i++) {
        hv_smmu_init();
        hv_u16 sid = 0;
        hv_smmu_allocate_streamid(BENCH_PARTITION_ID, &sid);
    }

    /* Measured iterations. */
    long long total_ns = 0;
    for (int i = 0; i < BENCH_ITERS; i++) {
        hv_smmu_init();
        hv_u16 sid = 0;

        long long t0 = hv_now_ns();
        (void)hv_smmu_allocate_streamid(BENCH_PARTITION_ID, &sid);
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

    /* Restore global state so subsequent calls using g_sid still work. */
    bench_setup();

    return r;
}

/* -----------------------------------------------------------------------
 * Print human-readable table.
 * ----------------------------------------------------------------------- */
static void print_table(bench_result_t *results, int n)
{
    printf("\n%-35s  %8s  %8s  %8s  %s\n",
           "benchmark", "min(ns)", "mean(ns)", "max(ns)", "iters");
    printf("%-35s  %8s  %8s  %8s  %s\n",
           "-----------------------------------",
           "--------", "--------", "--------", "----------");
    for (int i = 0; i < n; i++) {
        printf("%-35s  %8lld  %8lld  %8lld  %lld\n",
               results[i].name,
               results[i].min_ns, results[i].mean_ns,
               results[i].max_ns, results[i].iters);
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
    fprintf(f,
            "  \"description\": \"Haven SMMU DMA policy enforcement "
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
                results[i].name,
                results[i].min_ns, results[i].mean_ns,
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
    printf("=== Haven SMMU DMA Policy Enforcement Latency Benchmark ===\n");
    printf("    iterations per case: %d  warmup: %d\n\n",
           BENCH_ITERS, WARMUP_ITERS);

    bench_setup();

    bench_result_t results[NUM_CASES];
    int idx = 0;

    /*
     * Case 1 — streamid lookup (hot): DMA address inside the configured
     * window. The most common case on every permitted DMA transaction.
     */
    results[idx++] = run_check_case("smmu_check_access_hot",
                                    DMA_IOVA_BASE + 0x00100000ULL,
                                    0x1000ULL,
                                    HV_DMA_RO);

    /*
     * Case 2 — window low boundary: DMA at first byte of the window.
     */
    results[idx++] = run_check_case("smmu_check_window_boundary_low",
                                    DMA_IOVA_BASE,
                                    0x1000ULL,
                                    HV_DMA_RO);

    /*
     * Case 3 — window high boundary: DMA at the last valid page.
     */
    results[idx++] = run_check_case("smmu_check_window_boundary_high",
                                    DMA_IOVA_BASE + DMA_WINDOW_SZ - 0x1000ULL,
                                    0x1000ULL,
                                    HV_DMA_RO);

    /*
     * Case 4 — access denied: DMA address outside the window.
     * Policy must reject this; measures the rejection fast path.
     */
    results[idx++] = run_check_case("smmu_check_access_denied",
                                    DMA_IOVA_BASE + DMA_WINDOW_SZ + 0x1000ULL,
                                    0x1000ULL,
                                    HV_DMA_RO);

    /*
     * Case 5 — StreamID allocation: cold path, called once per device
     * attach. Measures the hv_smmu_allocate_streamid() overhead.
     */
    results[idx++] = run_alloc_case();

    print_table(results, NUM_CASES);
    write_json(results, NUM_CASES, OUT_PATH);

    return 0;
}
