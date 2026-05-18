/**
 * Per-partition timer deadline enforcement interface.
 *
 * Enforces per-partition timer ownership: each partition holds at most
 * one active deadline at a time. An expired deadline must be explicitly
 * acknowledged before a new one can be registered. This prevents deadline
 * hiding and ensures temporal isolation is fail-closed.
 *
 * Isolation invariants enforced:
 *   - Partition 0 is reserved and always rejected.
 *   - A deadline of 0 ns is invalid.
 *   - Only one active deadline per partition.
 *   - A new deadline cannot be set while the current one has expired but
 *     not yet been acknowledged (fail-closed guard).
 *   - Cancellation does not implicitly acknowledge an expired deadline.
 *
 * @file include/haven/timer.h
 */

#ifndef HAVEN_TIMER_H
#define HAVEN_TIMER_H

#include <haven/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Maximum partition IDs tracked by the timer module.
 * Matches HV_MAX_PARTITIONS convention (0 reserved, 1..255 valid).
 */
#define HV_MAX_TIMER_PARTITIONS 256U

    /**
     * Initialize the timer subsystem.
     *
     * Clears all partition deadline state. Must be called before any other
     * timer function.
     *
     * @return HV_OK on success.
     */
    hv_status_t hv_timer_init(void);

    /**
     * Set a deadline for a partition.
     *
     * The deadline is an absolute timestamp in nanoseconds. A partition may
     * only hold one active deadline. If the current deadline has expired but
     * not yet been acknowledged, this call returns HV_EPERM (fail-closed).
     *
     * @param partition_id  Owning partition (1–255; 0 is invalid).
     * @param deadline_ns   Absolute deadline timestamp (must be > 0).
     *
     * @return HV_OK      Deadline set.
     * @return HV_EINVAL  partition_id == 0 or deadline_ns == 0.
     * @return HV_EPERM   Partition has an unacknowledged expired deadline.
     * @return HV_ENOSPC  Partition already has an active (non-expired) deadline.
     */
    hv_status_t hv_timer_set_deadline(hv_u32 partition_id, hv_u64 deadline_ns);

    /**
     * Check whether a partition's deadline has expired.
     *
     * Compares the stored deadline against @p now_ns. If the deadline has
     * been reached or passed, sets @p *expired to 1; otherwise 0.
     * Does not automatically acknowledge the expiry.
     *
     * @param partition_id  Partition to check (1–255).
     * @param now_ns        Current time in nanoseconds.
     * @param expired       Output: set to 1 if expired, 0 if not.
     *
     * @return HV_OK      Check completed (see *expired).
     * @return HV_EINVAL  partition_id == 0, now_ns == 0, or expired == NULL.
     * @return HV_EPERM   Partition has no active deadline.
     */
    hv_status_t hv_timer_check_deadline(hv_u32 partition_id, hv_u64 now_ns,
                                        int *expired);

    /**
     * Acknowledge an expired deadline.
     *
     * Clears the expired state so the partition can register a new deadline.
     * Returns HV_EPERM if the deadline has not yet expired (early ack denied).
     *
     * @param partition_id  Partition whose deadline to acknowledge (1–255).
     *
     * @return HV_OK      Expired deadline acknowledged.
     * @return HV_EINVAL  partition_id == 0.
     * @return HV_EPERM   No active deadline, or deadline has not expired yet.
     */
    hv_status_t hv_timer_acknowledge(hv_u32 partition_id);

    /**
     * Cancel an active deadline.
     *
     * Removes a pending (non-expired) deadline. If the deadline has already
     * expired, cancel returns HV_EPERM — the caller must acknowledge first.
     * This prevents logic that silently swallows an expiry event.
     *
     * @param partition_id  Partition whose deadline to cancel (1–255).
     *
     * @return HV_OK      Deadline cancelled.
     * @return HV_EINVAL  partition_id == 0.
     * @return HV_EPERM   No active deadline, or deadline already expired
     *                    (must acknowledge before cancelling).
     */
    hv_status_t hv_timer_cancel(hv_u32 partition_id);

#ifdef __cplusplus
}
#endif

#endif /* HAVEN_TIMER_H */
