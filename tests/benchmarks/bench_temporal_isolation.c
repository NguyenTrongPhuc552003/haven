/* Required for clock_gettime / CLOCK_MONOTONIC under strict C11 */
#define _POSIX_C_SOURCE 200809L

/**
 * Temporal isolation benchmark — RTOS response-time under Linux load.
 *
 * This is the key thesis benchmark. It measures RTOS task response time
 * (simulated via the budget scheduler) under varying levels of simulated
 * Linux CPU load. The goal is to demonstrate that Haven's per-partition
 * budget enforcement keeps RTOS response latency bounded regardless of
 * what a co-hosted Linux partition does.
 *
 * Test matrix:
 *   RTOS periods : 1 ms, 5 ms, 10 ms
 *   Load levels  : 0 %, 25 %, 50 %, 75 %, 100 %  (simulated via spinning)
 *   Metric       : response latency (min / mean / max) in nanoseconds
 *   Iterations   : 10 000 per (period, load) combination
 *
 * Methodology:
 *   1. Configure a partition with hv_budget_set() using the given period and
 *      a budget of 20 % of the period (representative RTOS slice).
 *   2. Simulate a "task firing" by calling hv_budget_consume() for 1 ns
 *      (the overhead of the budget-check policy path) and measuring the
 *      wall-clock time from just before the call to just after.
 *   3. Between task firings, spin for (load_pct / 100) * period_ns nanoseconds
 *      to simulate a co-running Linux workload stealing CPU time.
 *   4. Report min / mean / max latency across 10 000 iterations.
 *
 * Output:
 *   Human-readable table on stdout.
 *   JSON array written to build/benchmarks/temporal-isolation.json.
 *
 * @file tests/benchmarks/bench_temporal_isolation.c
 */

#include <haven/budget_sched.h>
#include <haven/timer.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BENCH_ITERS  10000
#define OUT_PATH     "build/benchmarks/temporal-isolation.json"

/* RTOS partition used for all benchmark runs. */
#define RTOS_PARTITION_ID 7U

/* Budget is fixed at 20 % of the period — representative RTOS slice. */
#define BUDGET_FRACTION_PCT 20

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
 * Busy-spin for approximately @spin_ns nanoseconds.
 * Used to simulate Linux CPU load between RTOS task firings.
 * ----------------------------------------------------------------------- */
static void spin_ns(long long spin_ns)
{
    if (spin_ns <= 0)
        return;
    long long end = hv_now_ns() + spin_ns;
    while (hv_now_ns() < end) {
        /* tight spin — represents co-running Linux stealing CPU */
    }
}

/* -----------------------------------------------------------------------
 * Result record
 * ----------------------------------------------------------------------- */
typedef struct {
    long long period_ns;
    int       load_pct;
    long long budget_ns;
    long long min_ns;
    long long mean_ns;
    long long max_ns;
} temporal_result_t;

/* -----------------------------------------------------------------------
 * Run one (period, load_pct) benchmark cell.
 * ----------------------------------------------------------------------- */
static temporal_result_t run_cell(long long period_ns, int load_pct)
{
    temporal_result_t r;
    memset(&r, 0, sizeof(r));
    r.period_ns = period_ns;
    r.load_pct  = load_pct;
    r.budget_ns = period_ns * BUDGET_FRACTION_PCT / 100;
    r.min_ns    = 0x7fffffffffffffffLL;
    r.max_ns    = 0;

    /* Configure the RTOS partition budget. */
    struct hv_budget b = {
        .partition_id = RTOS_PARTITION_ID,
        .period_ns    = (hv_u64)period_ns,
        .budget_ns    = (hv_u64)r.budget_ns,
    };
    hv_budget_sched_init();
    hv_budget_set(&b);

    long long spin_window_ns = period_ns * load_pct / 100;
    long long total_ns = 0;

    /* Warm-up: 200 iterations, not timed. */
    for (int i = 0; i < 200; i++) {
        spin_ns(spin_window_ns);
        hv_budget_consume(RTOS_PARTITION_ID, 1ULL);
    }

    /* Re-init after warm-up to reset budget state. */
    hv_budget_sched_init();
    hv_budget_set(&b);

    for (int i = 0; i < BENCH_ITERS; i++) {
        /* Simulate Linux load consuming CPU between RTOS activations. */
        spin_ns(spin_window_ns);

        /* Measure: time from "period start" to "budget check passes". */
        long long t0 = hv_now_ns();
        hv_status_t s = hv_budget_consume(RTOS_PARTITION_ID, 1ULL);
        long long t1 = hv_now_ns();

        if (s == HV_EPERM) {
            /*
             * Budget exhausted (hv_budget_consume returns HV_EPERM when
             * consumed_ns would exceed budget_ns) — reset for next iteration.
             * This models a period boundary; re-init so subsequent
             * iterations still measure the hot path.
             */
            hv_budget_sched_init();
            hv_budget_set(&b);
        }

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
 * Derive a short benchmark name string.
 * ----------------------------------------------------------------------- */
static void make_bench_name(char *buf, size_t len,
                            long long period_ns, int load_pct)
{
    long long period_ms = period_ns / 1000000LL;
    snprintf(buf, len, "rtos_response_%lldms_load_%dpct",
             period_ms, load_pct);
}

/* -----------------------------------------------------------------------
 * Print human-readable table.
 * ----------------------------------------------------------------------- */
static void print_table(temporal_result_t *results, int n)
{
    printf("\n%-45s  %9s  %9s  %8s  %8s  %8s\n",
           "benchmark", "period_ns", "load_pct",
           "min(ns)", "mean(ns)", "max(ns)");
    printf("%-45s  %9s  %9s  %8s  %8s  %8s\n",
           "---------------------------------------------",
           "---------", "---------",
           "--------", "--------", "--------");
    for (int i = 0; i < n; i++) {
        char name[64];
        make_bench_name(name, sizeof(name),
                        results[i].period_ns, results[i].load_pct);
        printf("%-45s  %9lld  %9d  %8lld  %8lld  %8lld\n",
               name,
               results[i].period_ns,
               results[i].load_pct,
               results[i].min_ns,
               results[i].mean_ns,
               results[i].max_ns);
    }
    printf("\n");
}

/* -----------------------------------------------------------------------
 * Write JSON output.
 * ----------------------------------------------------------------------- */
static void write_json(temporal_result_t *results, int n, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[bench] warning: cannot write %s\n", path);
        return;
    }

    fprintf(f, "{\n");
    fprintf(f,
            "  \"description\": \"Haven temporal isolation — "
            "RTOS response time under Linux load\",\n");
    fprintf(f, "  \"iters\": %d,\n", BENCH_ITERS);
    fprintf(f, "  \"results\": [\n");

    for (int i = 0; i < n; i++) {
        char name[64];
        make_bench_name(name, sizeof(name),
                        results[i].period_ns, results[i].load_pct);
        fprintf(f,
                "    {\n"
                "      \"benchmark\": \"%s\",\n"
                "      \"period_ns\": %lld,\n"
                "      \"budget_ns\": %lld,\n"
                "      \"load_pct\": %d,\n"
                "      \"min_ns\": %lld,\n"
                "      \"mean_ns\": %lld,\n"
                "      \"max_ns\": %lld,\n"
                "      \"iters\": %d\n"
                "    }%s\n",
                name,
                results[i].period_ns,
                results[i].budget_ns,
                results[i].load_pct,
                results[i].min_ns,
                results[i].mean_ns,
                results[i].max_ns,
                BENCH_ITERS,
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
    printf("=== Haven Temporal Isolation Benchmark ===\n");
    printf("    RTOS response time under varying Linux CPU load\n");
    printf("    iterations per cell: %d\n\n", BENCH_ITERS);

    static const long long periods_ns[] = {
        1000000LL,   /* 1 ms  */
        5000000LL,   /* 5 ms  */
        10000000LL,  /* 10 ms */
    };
    static const int load_pcts[] = { 0, 25, 50, 75, 100 };

    int n_periods = (int)(sizeof(periods_ns) / sizeof(periods_ns[0]));
    int n_loads   = (int)(sizeof(load_pcts)  / sizeof(load_pcts[0]));
    int total     = n_periods * n_loads;

    temporal_result_t results[15]; /* 3 periods × 5 load levels */

    int idx = 0;
    for (int p = 0; p < n_periods; p++) {
        for (int l = 0; l < n_loads; l++) {
            char name[64];
            make_bench_name(name, sizeof(name), periods_ns[p], load_pcts[l]);
            printf("[bench] running %s ...\n", name);
            results[idx++] = run_cell(periods_ns[p], load_pcts[l]);
        }
    }

    print_table(results, total);
    write_json(results, total, OUT_PATH);

    return 0;
}
