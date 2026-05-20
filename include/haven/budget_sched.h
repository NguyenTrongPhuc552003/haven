#ifndef HAVEN_BUDGET_SCHED_H
#define HAVEN_BUDGET_SCHED_H

#include "types.h"

struct hv_budget {
	hv_u32 partition_id;
	hv_u64 period_ns;
	hv_u64 budget_ns;
};

hv_status_t hv_budget_sched_init(void);
hv_status_t hv_budget_set(const struct hv_budget *budget);
hv_status_t hv_budget_consume(hv_u32 partition_id, hv_u64 delta_ns);
hv_status_t hv_budget_reset_period(hv_u64 now_ns);

/*
 * hv_budget_watchdog_check - self-check watchdog for stale periods.
 *
 * Called periodically (e.g. from the EL2 timer handler) with the current
 * timestamp.  For each configured partition, if an entire period has
 * elapsed since the last reset without the scheduler calling
 * hv_budget_reset_period(), this function:
 *   1. Logs a warning to the hypervisor UART.
 *   2. Auto-resets the consumed counter so the partition is not
 *      permanently stalled.
 *
 * Returns the number of partitions whose budgets were auto-reset, or
 * HV_EINVAL if now_ns is invalid.
 */
hv_status_t hv_budget_watchdog_check(hv_u64 now_ns, hv_u32 *resets_out);

#endif
