#include "haven/budget_sched.h"
#include "haven/types.h"

extern void hv_printk(const char *fmt, ...);

#define HV_MAX_BUDGET_PARTITIONS 256U
/* Multiplier: trigger watchdog after this many missed periods */
#define HV_BUDGET_WATCHDOG_PERIODS 2U

struct hv_budget_state {
	hv_u64 period_ns;
	hv_u64 budget_ns;
	hv_u64 consumed_ns;
	hv_u8 configured;
};

static struct hv_budget_state budget_state[HV_MAX_BUDGET_PARTITIONS];
static hv_u64 budget_last_reset_ns[HV_MAX_BUDGET_PARTITIONS];
static hv_u64 budget_last_observed_ns;

hv_status_t hv_budget_sched_init(void)
{
	hv_u32 i;

	for (i = 0U; i < HV_MAX_BUDGET_PARTITIONS; ++i) {
		budget_state[i].period_ns = 0U;
		budget_state[i].budget_ns = 0U;
		budget_state[i].consumed_ns = 0U;
		budget_state[i].configured = 0U;
	}

	for (i = 0U; i < HV_MAX_BUDGET_PARTITIONS; ++i) {
		budget_last_reset_ns[i] = 0U;
	}
	budget_last_observed_ns = 0U;

	return HV_OK;
}

hv_status_t hv_budget_set(const struct hv_budget *budget)
{
	if (budget == NULL || budget->partition_id == 0U) {
		return HV_EINVAL;
	}

	if (budget->partition_id >= HV_MAX_BUDGET_PARTITIONS) {
		return HV_ENOSPC;
	}

	if (budget->budget_ns == 0U || budget->period_ns < budget->budget_ns) {
		return HV_EINVAL;
	}

	budget_state[budget->partition_id].period_ns = budget->period_ns;
	budget_state[budget->partition_id].budget_ns = budget->budget_ns;
	budget_state[budget->partition_id].consumed_ns = 0U;
	budget_state[budget->partition_id].configured = 1U;

	return HV_OK;
}

hv_status_t hv_budget_consume(hv_u32 partition_id, hv_u64 delta_ns)
{
	hv_u64 next;

	if (partition_id == 0U || delta_ns == 0U) {
		return HV_EINVAL;
	}

	if (partition_id >= HV_MAX_BUDGET_PARTITIONS) {
		return HV_ENOSPC;
	}

	if (budget_state[partition_id].configured == 0U) {
		return HV_EPERM;
	}

	next = budget_state[partition_id].consumed_ns + delta_ns;
	if (next < budget_state[partition_id].consumed_ns) {
		return HV_EINVAL;
	}

	if (next > budget_state[partition_id].budget_ns) {
		return HV_EPERM;
	}

	budget_state[partition_id].consumed_ns = next;

	return HV_OK;
}

hv_status_t hv_budget_reset_period(hv_u64 now_ns)
{
	hv_u32 i;
	hv_u64 elapsed;

	if (now_ns == 0U) {
		return HV_EINVAL;
	}

	if (budget_last_observed_ns != 0U && now_ns < budget_last_observed_ns) {
		return HV_EINVAL;
	}

	for (i = 0U; i < HV_MAX_BUDGET_PARTITIONS; ++i) {
		if (budget_state[i].configured == 0U) {
			continue;
		}

		if (budget_last_reset_ns[i] != 0U &&
		    now_ns < budget_last_reset_ns[i]) {
			return HV_EINVAL;
		}

		if (budget_last_reset_ns[i] == 0U) {
			elapsed = now_ns;
		} else {
			elapsed = now_ns - budget_last_reset_ns[i];
		}

		if (elapsed >= budget_state[i].period_ns) {
			budget_state[i].consumed_ns = 0U;
			budget_last_reset_ns[i] = now_ns;
		}
	}
	budget_last_observed_ns = now_ns;

	return HV_OK;
}

/*
 * hv_budget_watchdog_check - detect and recover from stale budget periods.
 *
 * If a partition's period has elapsed HV_BUDGET_WATCHDOG_PERIODS times
 * without a reset, log a warning and auto-reset the consumed counter.
 * This prevents a scheduler bug from permanently starving a partition.
 *
 * now_ns  - current monotonic timestamp from hv_arch_timer_now_ns().
 * resets_out - set to the count of auto-resets performed (may be NULL).
 */
hv_status_t hv_budget_watchdog_check(hv_u64 now_ns, hv_u32 *resets_out)
{
	hv_u32 i;
	hv_u32 resets = 0U;

	if (now_ns == 0U)
		return HV_EINVAL;

	for (i = 0U; i < HV_MAX_BUDGET_PARTITIONS; ++i) {
		hv_u64 elapsed;
		hv_u64 stale_threshold;

		if (budget_state[i].configured == 0U)
			continue;
		if (budget_state[i].period_ns == 0U)
			continue;

		/* Compute elapsed time since last reset */
		if (budget_last_reset_ns[i] == 0U) {
			/* Never reset: count from first observation */
			elapsed = now_ns;
		} else if (now_ns < budget_last_reset_ns[i]) {
			/* Clock appears to have gone backwards; skip safely */
			continue;
		} else {
			elapsed = now_ns - budget_last_reset_ns[i];
		}

		/* Watchdog fires after HV_BUDGET_WATCHDOG_PERIODS missed resets */
		stale_threshold = budget_state[i].period_ns *
				  (hv_u64)HV_BUDGET_WATCHDOG_PERIODS;

		if (elapsed >= stale_threshold) {
			hv_printk("[budget] watchdog: partition %u period "
				  "overdue by %llu ns (period=%llu ns), "
				  "auto-resetting budget\n",
				  i, elapsed - budget_state[i].period_ns,
				  budget_state[i].period_ns);
			budget_state[i].consumed_ns = 0U;
			budget_last_reset_ns[i] = now_ns;
			resets++;
		}
	}

	if (resets_out != NULL)
		*resets_out = resets;

	return HV_OK;
}
