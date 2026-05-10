/**
 * FreeRTOS guest integration interface for hypervisor.
 *
 * Provides support for FreeRTOS RTOS guests running in isolated partitions,
 * including task scheduling support, memory regions, interrupt handling,
 * and timer services.
 *
 * Features:
 *   - FreeRTOS task context switching support
 *   - CPU time budget allocation per RTOS partition
 *   - Shared memory regions for inter-partition communication
 *   - Timer interrupt delivery to RTOS kernel
 *   - Hardware interrupt routing to FreeRTOS handlers
 *
 * @file include/haven/freertos_integration.h
 */

#ifndef HAVEN_FREERTOS_INTEGRATION_H
#define HAVEN_FREERTOS_INTEGRATION_H

#include <haven/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum FreeRTOS tasks per partition.
 */
#define HV_MAX_FREERTOS_TASKS_PER_PARTITION 64

/**
 * Maximum FreeRTOS partitions (RTOS guests).
 */
#define HV_MAX_FREERTOS_PARTITIONS 16

/**
 * Maximum shared memory regions.
 */
#define HV_MAX_SHARED_REGIONS 32

/**
 * FreeRTOS task state enumeration.
 */
typedef enum {
	HV_TASK_READY = 0,      /**< Task ready to run */
	HV_TASK_RUNNING = 1,    /**< Task currently running */
	HV_TASK_BLOCKED = 2,    /**< Task blocked on event/timer */
	HV_TASK_SUSPENDED = 3,  /**< Task suspended by scheduler */
	HV_TASK_DELETED = 4,    /**< Task deleted */
} hv_task_state_t;

/**
 * Shared memory access permissions.
 */
typedef enum {
	HV_SHARED_RO = 1,  /**< Read-only shared region */
	HV_SHARED_WO = 2,  /**< Write-only shared region */
	HV_SHARED_RW = 3,  /**< Read-write shared region */
} hv_shared_access_t;

/**
 * FreeRTOS partition configuration.
 */
typedef struct {
	hv_u32 partition;
	hv_u32 cpu_cores;        /**< CPU cores assigned to RTOS partition */
	hv_u64 time_budget_us;   /**< Time budget per scheduling period (microseconds) */
	hv_u32 max_tasks;        /**< Maximum tasks supported */
	hv_u32 timer_frequency;  /**< System timer frequency (Hz) */
} hv_freertos_config_t;

/**
 * Shared memory region descriptor.
 */
typedef struct {
	hv_u32 region_id;
	hv_u32 partition1;
	hv_u32 partition2;
	hv_u64 start_addr;        /**< Shared address */
	hv_u64 size;              /**< Region size in bytes */
	hv_shared_access_t access; /**< Access permissions */
	int enabled;              /**< Region active flag */
} hv_shared_region_t;

/**
 * Task context for context switching.
 */
typedef struct {
	hv_u32 task_id;
	hv_u32 priority;
	hv_task_state_t state;
	hv_u64 sp;     /**< Stack pointer */
	hv_u64 pc;     /**< Program counter */
	hv_u64 x0_x30[31];  /**< ARM64 general purpose registers */
} hv_task_context_t;

/**
 * Initialize FreeRTOS integration subsystem.
 *
 * Must be called once at hypervisor startup before any RTOS partition creation.
 *
 * @return HV_OK on success
 */
hv_status_t hv_freertos_init(void);

/**
 * Create and configure a FreeRTOS guest partition.
 *
 * Allocates resources for an RTOS guest partition with specified configuration.
 *
 * @param config   RTOS partition configuration
 * @param partition_id  Output: allocated partition ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if config invalid or partition_id NULL
 * @return HV_ENOSPC if max partitions reached
 */
hv_status_t hv_freertos_create_partition(const hv_freertos_config_t *config, hv_u32 *partition_id);

/**
 * Register a FreeRTOS task for tracking.
 *
 * Hypervisor tracks tasks for scheduling and context switching support.
 *
 * @param partition  RTOS partition ID
 * @param task_id    FreeRTOS task handle/ID
 * @param priority   Task priority (lower = higher priority in some RTOSes)
 * @param stack_ptr  Task stack pointer
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition or task_id invalid
 * @return HV_EPERM if partition not allocated
 * @return HV_ENOSPC if max tasks reached
 */
hv_status_t hv_freertos_register_task(
	hv_u32 partition,
	hv_u32 task_id,
	hv_u32 priority,
	hv_u64 stack_ptr
);

/**
 * Save task context on preemption.
 *
 * Saves registers and state before switching tasks or preempting RTOS guest.
 *
 * @param partition  RTOS partition ID
 * @param ctx        Task context to save
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition or ctx NULL
 * @return HV_EPERM if partition not allocated
 */
hv_status_t hv_freertos_save_context(hv_u32 partition, hv_task_context_t *ctx);

/**
 * Restore task context on resume.
 *
 * Restores registers and state when resuming RTOS guest.
 *
 * @param partition  RTOS partition ID
 * @param ctx        Task context to restore
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition or ctx NULL
 * @return HV_EPERM if partition not allocated
 */
hv_status_t hv_freertos_restore_context(hv_u32 partition, const hv_task_context_t *ctx);

/**
 * Allocate shared memory region between two partitions.
 *
 * Creates a shared memory region for inter-partition communication.
 * Can be used between two FreeRTOS partitions, or FreeRTOS and Linux.
 *
 * @param partition1  First partition ID
 * @param partition2  Second partition ID
 * @param size        Region size in bytes (must be page-aligned)
 * @param access      Access permissions
 * @param region_id   Output: shared region ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partitions invalid, size misaligned, or region_id NULL
 * @return HV_EPERM if partitions not allocated
 * @return HV_ENOSPC if max shared regions reached
 */
hv_status_t hv_freertos_allocate_shared_region(
	hv_u32 partition1,
	hv_u32 partition2,
	hv_u64 size,
	hv_shared_access_t access,
	hv_u32 *region_id
);

/**
 * Get shared memory region address for partition.
 *
 * Returns the mapped address of a shared region as seen by the partition.
 *
 * @param region_id     Shared region ID
 * @param partition     Partition ID requesting address
 * @param address       Output: mapped address in partition's address space
 *
 * @return HV_OK on success
 * @return HV_EINVAL if region_id invalid or address NULL
 * @return HV_EPERM if partition doesn't have access to region
 */
hv_status_t hv_freertos_get_shared_address(hv_u32 region_id, hv_u32 partition, hv_u64 *address);

/**
 * Deliver timer interrupt to FreeRTOS partition.
 *
 * Injects a system timer interrupt for FreeRTOS kernel tick/time slicing.
 *
 * @param partition  RTOS partition ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition invalid
 * @return HV_EPERM if partition not allocated
 */
hv_status_t hv_freertos_deliver_timer(hv_u32 partition);

/**
 * Route hardware interrupt to FreeRTOS handler.
 *
 * Configures interrupt routing so hardware IRQs are delivered to RTOS partition.
 *
 * @param partition  RTOS partition ID
 * @param irq        Hardware IRQ number
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition or irq invalid
 * @return HV_EPERM if partition not allocated or IRQ already routed
 */
hv_status_t hv_freertos_route_irq(hv_u32 partition, hv_u32 irq);

/**
 * Unroute hardware interrupt from FreeRTOS partition.
 *
 * Removes IRQ routing for a partition.
 *
 * @param partition  RTOS partition ID
 * @param irq        Hardware IRQ number
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition or irq invalid
 * @return HV_EPERM if IRQ not routed to this partition
 */
hv_status_t hv_freertos_unroute_irq(hv_u32 partition, hv_u32 irq);

/**
 * Destroy FreeRTOS partition and free resources.
 *
 * Terminates RTOS partition, cleans up tasks, and frees allocated resources.
 *
 * @param partition  RTOS partition ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition invalid
 * @return HV_EPERM if partition not allocated
 */
hv_status_t hv_freertos_destroy_partition(hv_u32 partition);

/**
 * Get FreeRTOS partition statistics.
 *
 * Returns runtime statistics: tasks created, context switches, interrupts, etc.
 *
 * @param partition       RTOS partition ID
 * @param task_count      Output: number of registered tasks
 * @param context_switches Output: total context switches
 * @param timer_ticks     Output: total timer interrupts delivered
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition invalid or output pointers NULL
 * @return HV_EPERM if partition not allocated
 */
hv_status_t hv_freertos_get_stats(
	hv_u32 partition,
	hv_u32 *task_count,
	hv_u64 *context_switches,
	hv_u64 *timer_ticks
);

#ifdef __cplusplus
}
#endif

#endif /* HAVEN_FREERTOS_INTEGRATION_H */
