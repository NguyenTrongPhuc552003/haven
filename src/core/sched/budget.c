#include "haven/budget_sched.h"

#define HV_MAX_BUDGET_PARTITIONS 256U

struct hv_budget_state {
  hv_u64 period_ns;
  hv_u64 budget_ns;
  hv_u64 consumed_ns;
  hv_u8 configured;
};

static struct hv_budget_state budget_state[HV_MAX_BUDGET_PARTITIONS];

hv_status_t hv_budget_sched_init(void) {
  hv_u32 i;

  for (i = 0U; i < HV_MAX_BUDGET_PARTITIONS; ++i) {
    budget_state[i].period_ns = 0U;
    budget_state[i].budget_ns = 0U;
    budget_state[i].consumed_ns = 0U;
    budget_state[i].configured = 0U;
  }

  return HV_OK;
}

hv_status_t hv_budget_set(const struct hv_budget *budget) {
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

hv_status_t hv_budget_consume(hv_u32 partition_id, hv_u64 delta_ns) {
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

hv_status_t hv_budget_reset_period(hv_u64 now_ns) {
  hv_u32 i;

  if (now_ns == 0U) {
    return HV_EINVAL;
  }

  for (i = 0U; i < HV_MAX_BUDGET_PARTITIONS; ++i) {
    if (budget_state[i].configured != 0U) {
      budget_state[i].consumed_ns = 0U;
    }
  }

  return HV_OK;
}
