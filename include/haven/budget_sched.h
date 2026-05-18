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

#endif
