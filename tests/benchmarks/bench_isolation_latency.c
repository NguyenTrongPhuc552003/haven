/* Required for clock_gettime / CLOCK_MONOTONIC under strict C11 */
#define _POSIX_C_SOURCE 200809L

/**
 * Isolation hot-path latency benchmark.
 *
 * Measures the per-call latency (nanoseconds) of the six enforcement
 * operations that appear on every partition-switch or device-access
 * critical path. These are the operations whose execution time bounds
 * the worst-case overhead of the hypervisor isolation layer.
 *
 * Hot paths benchmarked:
 *   1. hv_stage2_partition_contains_pa  — spatial containment check
 *   2. hv_irq_is_owned_by               — IRQ ownership query
 *   3. hv_budget_consume                — CPU budget accounting
 *   4. hv_smmu_check_dma_access         — DMA access check
 *   5. hv_timer_check_deadline          — timer deadline poll
 *   6. hv_iommu_check_group             — IOMMU group ownership query
 *
 * Output:
 *   - Human-readable table on stdout.
 *   - JSON results written to build/benchmarks/isolation-latency.json.
 *
 * Methodology:
 *   - ITERATIONS = 100 000 per benchmark.
 *   - Warm-up = 1 000 iterations (discarded).
 *   - Timing via clock_gettime(CLOCK_MONOTONIC).
 *   - Reported values: min, mean, max (all in nanoseconds).
 *   - Each measurement is the wall-clock delta around a single call.
 *
 * @file tests/benchmarks/bench_isolation_latency.c
 */

#include <assert.h>
#include <haven/budget_sched.h>
#include <haven/iommu.h>
#include <haven/irq_ownership.h>
#include <haven/smmu.h>
#include <haven/stage2.h>
#include <haven/timer.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define WARMUP_ITERS 1000
#define BENCH_ITERS 100000
#define NUM_BENCHES 6
#define OUT_PATH "build/benchmarks/isolation-latency.json"

/* -----------------------------------------------------------------------
 * Timing primitive
 * ----------------------------------------------------------------------- */
static long long hv_now_ns(void) {
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
 * Generic timing loop
 *
 * fn_setup()   — called once before warmup (module/state setup)
 * fn_reset()   — called before each iteration batch to reset mutable state
 * fn_call()    — the single hot-path expression to time (returns hv_status_t)
 * fn_recover() — called when fn_call() returns non-OK to restore state
 * ----------------------------------------------------------------------- */
typedef void (*bench_setup_fn)(void);
typedef void (*bench_reset_fn)(void);
typedef hv_status_t (*bench_call_fn)(void);
typedef void (*bench_recover_fn)(void);

static bench_result_t run_bench(const char *name, bench_setup_fn setup,
                                bench_reset_fn reset, bench_call_fn call,
                                bench_recover_fn recover) {
  bench_result_t r;
  memset(&r, 0, sizeof(r));
  r.name = name;
  r.iters = BENCH_ITERS;
  r.min_ns = 0x7fffffffffffffffLL;
  r.max_ns = 0;

  if (setup)
    setup();

  /* Warm-up — not timed. */
  for (int i = 0; i < WARMUP_ITERS; i++) {
    if (reset)
      reset();
    hv_status_t s = call();
    if (s != HV_OK && recover)
      recover();
  }

  /* Measured iterations. */
  long long total_ns = 0;
  for (int i = 0; i < BENCH_ITERS; i++) {
    if (reset)
      reset();
    long long t0 = hv_now_ns();
    hv_status_t s = call();
    long long t1 = hv_now_ns();
    (void)s;
    if (s != HV_OK && recover)
      recover();

    long long delta = t1 - t0;
    if (delta < 0)
      delta = 0; /* clock wrap guard */
    if (delta < r.min_ns)
      r.min_ns = delta;
    if (delta > r.max_ns)
      r.max_ns = delta;
    total_ns += delta;
  }

  r.mean_ns = total_ns / BENCH_ITERS;
  return r;
}

/* ===================================================================== */
/* Benchmark 1: hv_stage2_partition_contains_pa                          */
/* ===================================================================== */
static void b1_setup(void) {
  static struct hv_mem_region reg = {
      .ipa_base = 0x40000000,
      .pa_base = 0x40000000,
      .size = 0x04000000,
      .attrs = 0,
  };
  static struct hv_partition_mem cfg = {
      .partition_id = 1,
      .regions = &reg,
      .region_count = 1,
  };
  hv_stage2_init();
  hv_stage2_map_partition(&cfg);
}
static hv_status_t b1_call(void) {
  return hv_stage2_partition_contains_pa(1, 0x41000000, 0x1000);
}

/* ===================================================================== */
/* Benchmark 2: hv_irq_is_owned_by                                       */
/* ===================================================================== */
static void b2_setup(void) {
  static struct hv_irq_route route = {
      .irq_id = 48,
      .owner_partition_id = 1,
      .target_cpu = 0,
  };
  hv_irq_owner_init();
  hv_irq_assign(&route);
}
static hv_status_t b2_call(void) { return hv_irq_is_owned_by(48, 1); }

/* ===================================================================== */
/* Benchmark 3: hv_budget_consume                                        */
/* ===================================================================== */
static struct hv_budget b3_budget = {
    .partition_id = 3,
    .period_ns = 1000000000ULL,
    .budget_ns = 1000000000ULL,
};
static void b3_setup(void) {
  hv_budget_sched_init();
  hv_budget_set(&b3_budget);
}
static hv_status_t b3_call(void) { return hv_budget_consume(3, 1ULL); }

/* ===================================================================== */
/* Benchmark 4: hv_smmu_check_dma_access                                 */
/* ===================================================================== */
static hv_u16 b4_sid = 0;
static void b4_setup(void) {
  static struct hv_mem_region p4_reg = {
      .ipa_base = 0x50000000,
      .pa_base = 0x50000000,
      .size = 0x02000000,
      .attrs = 0,
  };
  static struct hv_partition_mem p4_cfg = {
      .partition_id = 4,
      .regions = &p4_reg,
      .region_count = 1,
  };
  hv_smmu_init();
  hv_stage2_map_partition(&p4_cfg);
  hv_smmu_allocate_streamid(4, &b4_sid);
  hv_smmu_configure_dma_window(b4_sid, 0x50000000, 0x01000000, 0x50000000,
                               HV_DMA_RW);
}
static hv_status_t b4_call(void) {
  return hv_smmu_check_dma_access(b4_sid, 0x50100000, 0x1000, HV_DMA_RO);
}

/* ===================================================================== */
/* Benchmark 5: hv_timer_check_deadline                                  */
/* ===================================================================== */
static void b5_setup(void) {
  hv_timer_init();
  hv_timer_set_deadline(5, 9999999999999ULL); /* far future */
}
static int b5_expired = 0;
static hv_status_t b5_call(void) {
  return hv_timer_check_deadline(5, 1000ULL, &b5_expired);
}

/* ===================================================================== */
/* Benchmark 6: hv_iommu_check_group                                     */
/* ===================================================================== */
static void b6_setup(void) {
  hv_iommu_init();
  hv_iommu_assign_group(6, 1);
}
static hv_status_t b6_call(void) { return hv_iommu_check_group(6, 1); }

/* ===================================================================== */
/* Report and JSON output                                                 */
/* ===================================================================== */
static void print_table(bench_result_t *results, int n) {
  printf("\n%-40s  %8s  %8s  %8s  %s\n", "benchmark", "min(ns)", "mean(ns)",
         "max(ns)", "iters");
  printf("%-40s  %8s  %8s  %8s  %s\n",
         "----------------------------------------", "--------", "--------",
         "--------", "----------");
  for (int i = 0; i < n; i++) {
    printf("%-40s  %8lld  %8lld  %8lld  %lld\n", results[i].name,
           results[i].min_ns, results[i].mean_ns, results[i].max_ns,
           results[i].iters);
  }
  printf("\n");
}

static void write_json(bench_result_t *results, int n, const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) {
    fprintf(stderr, "[bench] warning: cannot write %s\n", path);
    return;
  }

  fprintf(f, "{\n");
  fprintf(
      f,
      "  \"description\": \"Haven isolation hot-path latency benchmark\",\n");
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
            results[i].max_ns, results[i].iters, (i < n - 1) ? "," : "");
  }

  fprintf(f, "  ]\n}\n");
  fclose(f);
  printf("[bench] results written to %s\n", path);
}

/* ===================================================================== */
/* main                                                                   */
/* ===================================================================== */
int main(void) {
  printf("=== Haven Isolation Hot-Path Latency Benchmark ===\n");
  printf("    iterations per bench: %d  warmup: %d\n\n", BENCH_ITERS,
         WARMUP_ITERS);

  bench_result_t results[NUM_BENCHES];
  int idx = 0;

  results[idx++] =
      run_bench("stage2_partition_contains_pa", b1_setup, NULL, b1_call, NULL);

  results[idx++] = run_bench("irq_is_owned_by", b2_setup, NULL, b2_call, NULL);

  results[idx++] = run_bench("budget_consume", b3_setup, NULL, b3_call, NULL);

  results[idx++] =
      run_bench("smmu_check_dma_access", b4_setup, NULL, b4_call, NULL);

  results[idx++] =
      run_bench("timer_check_deadline", b5_setup, NULL, b5_call, NULL);

  results[idx++] =
      run_bench("iommu_check_group", b6_setup, NULL, b6_call, NULL);

  assert(idx == NUM_BENCHES);

  print_table(results, NUM_BENCHES);
  write_json(results, NUM_BENCHES, OUT_PATH);

  return 0;
}
