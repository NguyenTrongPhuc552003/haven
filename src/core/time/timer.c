/**
 * Per-partition timer deadline enforcement implementation.
 *
 * @file src/core/time/timer.c
 */

#include <haven/timer.h>
#include <haven/string.h>

#ifdef HAVEN_ARCH_ARM64
extern hv_u64 hv_arch_timer_now(void);
extern void hv_arch_timer_set_deadline(hv_u64 deadline_ns);
extern void hv_arch_timer_cancel(void);
#endif

/**
 * Timer state for a single partition.
 */
typedef struct {
  hv_u64 deadline_ns; /**< Absolute deadline timestamp. 0 = not set. */
  int active;         /**< 1 if a deadline is set (pending or expired). */
  int expired;        /**< 1 if deadline has been reached but not acked. */
} partition_timer_t;

/**
 * Per-partition timer state table.
 */
static partition_timer_t timer_state[HV_MAX_TIMER_PARTITIONS];

#ifdef HAVEN_ARCH_ARM64
static void hv_timer_program_earliest_deadline(void) {
  hv_u64 earliest = 0;
  hv_u32 i;

  /* Partition 0 is reserved by the timer API contract and never active. */
  for (i = 1; i < HV_MAX_TIMER_PARTITIONS; ++i) {
    if (!timer_state[i].active) {
      continue;
    }

    if (earliest == 0 || timer_state[i].deadline_ns < earliest) {
      earliest = timer_state[i].deadline_ns;
    }
  }

  if (earliest != 0) {
    hv_arch_timer_set_deadline(earliest);
  } else {
    hv_arch_timer_cancel();
  }
}
#endif

hv_status_t hv_timer_init(void) {
  memset(timer_state, 0, sizeof(timer_state));
  return HV_OK;
}

hv_status_t hv_timer_set_deadline(hv_u32 partition_id, hv_u64 deadline_ns) {
  if (partition_id == 0 || partition_id >= HV_MAX_TIMER_PARTITIONS) {
    return HV_EINVAL;
  }

  if (deadline_ns == 0) {
    return HV_EINVAL;
  }

  partition_timer_t *t = &timer_state[partition_id];

  /*
   * Fail-closed: if the partition has an expired-but-unacknowledged
   * deadline, block the new set. The caller must acknowledge first.
   */
  if (t->active && t->expired) {
    return HV_EPERM;
  }

  /*
   * Only one active deadline per partition.
   */
  if (t->active) {
    return HV_ENOSPC;
  }

  t->deadline_ns = deadline_ns;
  t->active = 1;
  t->expired = 0;

#ifdef HAVEN_ARCH_ARM64
  hv_timer_program_earliest_deadline();
#endif

  return HV_OK;
}

hv_status_t hv_timer_check_deadline(hv_u32 partition_id, hv_u64 now_ns,
                                    int *expired) {
  if (partition_id == 0 || partition_id >= HV_MAX_TIMER_PARTITIONS) {
    return HV_EINVAL;
  }

  if (now_ns == 0 || expired == NULL) {
    return HV_EINVAL;
  }

  partition_timer_t *t = &timer_state[partition_id];

  if (!t->active) {
    return HV_EPERM;
  }

#ifdef HAVEN_ARCH_ARM64
  now_ns = hv_arch_timer_now();
#endif

  if (now_ns >= t->deadline_ns) {
    t->expired = 1;
    *expired = 1;
  } else {
    *expired = 0;
  }

  return HV_OK;
}

hv_status_t hv_timer_acknowledge(hv_u32 partition_id) {
  if (partition_id == 0 || partition_id >= HV_MAX_TIMER_PARTITIONS) {
    return HV_EINVAL;
  }

  partition_timer_t *t = &timer_state[partition_id];

  if (!t->active) {
    return HV_EPERM;
  }

  /*
   * Early acknowledgement denied: the deadline must actually have
   * expired before the caller can acknowledge it.
   */
  if (!t->expired) {
    return HV_EPERM;
  }

  t->deadline_ns = 0;
  t->active = 0;
  t->expired = 0;

#ifdef HAVEN_ARCH_ARM64
  hv_timer_program_earliest_deadline();
#endif

  return HV_OK;
}

hv_status_t hv_timer_cancel(hv_u32 partition_id) {
  if (partition_id == 0 || partition_id >= HV_MAX_TIMER_PARTITIONS) {
    return HV_EINVAL;
  }

  partition_timer_t *t = &timer_state[partition_id];

  if (!t->active) {
    return HV_EPERM;
  }

  /*
   * Expired deadline cannot be silently cancelled: the caller must
   * acknowledge the expiry explicitly before any new deadline work.
   * This prevents logic that swallows expiry events.
   */
  if (t->expired) {
    return HV_EPERM;
  }

  t->deadline_ns = 0;
  t->active = 0;
  t->expired = 0;

#ifdef HAVEN_ARCH_ARM64
  hv_timer_program_earliest_deadline();
#endif

  return HV_OK;
}
