#include "haven/budget_sched.h"

hv_status_t hv_budget_sched_init(void) {
  return HV_OK;
}

hv_status_t hv_budget_set(const struct hv_budget *budget) {
  if (budget == NULL || budget->partition_id == 0U) {
    return HV_EINVAL;
  }

  if (budget->budget_ns == 0U || budget->period_ns < budget->budget_ns) {
    return HV_EINVAL;
  }

  return HV_OK;
}

hv_status_t hv_budget_consume(hv_u32 partition_id, hv_u64 delta_ns) {
  if (partition_id == 0U || delta_ns == 0U) {
    return HV_EINVAL;
  }

  return HV_OK;
}

hv_status_t hv_budget_reset_period(hv_u64 now_ns) {
  if (now_ns == 0U) {
    return HV_EINVAL;
  }

  return HV_OK;
}
