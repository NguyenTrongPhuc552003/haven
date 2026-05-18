/**
 * FreeRTOS guest integration implementation.
 *
 * @file src/guest/rtos/freertos_integration.c
 */

#include <haven/freertos_integration.h>
#include <haven/string.h>

/**
 * FreeRTOS partition state.
 */
typedef struct {
	hv_u32 partition;
	hv_u32 cpu_cores;
	hv_u64 time_budget_us;
	hv_u32 max_tasks;
	hv_u32 timer_frequency;
	int allocated;
	hv_u32 task_count;
	hv_u64 context_switches;
	hv_u64 timer_ticks;
} hv_freertos_partition_t;

/**
 * Task tracking.
 */
typedef struct {
	hv_u32 task_id;
	hv_u32 partition;
	hv_u32 priority;
	hv_u64 stack_ptr;
	hv_task_state_t state;
	int allocated;
} hv_task_t;

/**
 * Global state.
 */
static hv_freertos_partition_t freertos_partitions[HV_MAX_FREERTOS_PARTITIONS] =
	{0};
static hv_task_t task_registry[HV_MAX_FREERTOS_TASKS_PER_PARTITION *
			       HV_MAX_FREERTOS_PARTITIONS] = {0};
static hv_shared_region_t shared_regions[HV_MAX_SHARED_REGIONS] = {0};
static hv_u16 partition_count = 0;
static hv_u16 task_count = 0;
static hv_u16 region_count = 0;

hv_status_t hv_freertos_init(void)
{
	memset(freertos_partitions, 0, sizeof(freertos_partitions));
	memset(task_registry, 0, sizeof(task_registry));
	memset(shared_regions, 0, sizeof(shared_regions));
	partition_count = 0;
	task_count = 0;
	region_count = 0;
	return HV_OK;
}

hv_status_t hv_freertos_create_partition(const hv_freertos_config_t *config,
					 hv_u32 *partition_id)
{
	if (config == NULL || partition_id == NULL) {
		return HV_EINVAL;
	}

	if (config->partition >= HV_MAX_FREERTOS_PARTITIONS) {
		return HV_EINVAL;
	}

	if (partition_count >= HV_MAX_FREERTOS_PARTITIONS) {
		return HV_ENOSPC;
	}

	for (hv_u32 i = 0; i < HV_MAX_FREERTOS_PARTITIONS; i++) {
		if (!freertos_partitions[i].allocated) {
			freertos_partitions[i].partition = config->partition;
			freertos_partitions[i].cpu_cores = config->cpu_cores;
			freertos_partitions[i].time_budget_us =
				config->time_budget_us;
			freertos_partitions[i].max_tasks = config->max_tasks;
			freertos_partitions[i].timer_frequency =
				config->timer_frequency;
			freertos_partitions[i].allocated = 1;
			freertos_partitions[i].task_count = 0;
			freertos_partitions[i].context_switches = 0;
			freertos_partitions[i].timer_ticks = 0;
			partition_count++;
			*partition_id = i;
			return HV_OK;
		}
	}

	return HV_ENOSPC;
}

hv_status_t hv_freertos_register_task(hv_u32 partition, hv_u32 task_id,
				      hv_u32 priority, hv_u64 stack_ptr)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS || task_id == 0) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	if (freertos_partitions[partition].task_count >=
	    freertos_partitions[partition].max_tasks) {
		return HV_ENOSPC;
	}

	/* Find available slot in registry. */
	for (hv_u32 i = 0; i < HV_MAX_FREERTOS_TASKS_PER_PARTITION *
				       HV_MAX_FREERTOS_PARTITIONS;
	     i++) {
		if (!task_registry[i].allocated) {
			task_registry[i].task_id = task_id;
			task_registry[i].partition = partition;
			task_registry[i].priority = priority;
			task_registry[i].stack_ptr = stack_ptr;
			task_registry[i].state = HV_TASK_READY;
			task_registry[i].allocated = 1;
			freertos_partitions[partition].task_count++;
			task_count++;
			return HV_OK;
		}
	}

	return HV_ENOSPC;
}

hv_status_t hv_freertos_save_context(hv_u32 partition, hv_task_context_t *ctx)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS || ctx == NULL) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	/* Context save is recorded; in real impl, would save to task TCB. */
	freertos_partitions[partition].context_switches++;

	return HV_OK;
}

hv_status_t hv_freertos_restore_context(hv_u32 partition,
					const hv_task_context_t *ctx)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS || ctx == NULL) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	/* Context restore recorded. */
	return HV_OK;
}

hv_status_t hv_freertos_allocate_shared_region(hv_u32 partition1,
					       hv_u32 partition2, hv_u64 size,
					       hv_shared_access_t access,
					       hv_u32 *region_id)
{
	if (partition1 >= HV_MAX_FREERTOS_PARTITIONS ||
	    partition2 >= HV_MAX_FREERTOS_PARTITIONS) {
		return HV_EINVAL;
	}

	if (region_id == NULL) {
		return HV_EINVAL;
	}

	if ((size & 0xFFF) != 0) {
		return HV_EINVAL; /* Must be page-aligned */
	}

	if (!freertos_partitions[partition1].allocated ||
	    !freertos_partitions[partition2].allocated) {
		return HV_EPERM;
	}

	if (region_count >= HV_MAX_SHARED_REGIONS) {
		return HV_ENOSPC;
	}

	for (hv_u32 i = 0; i < HV_MAX_SHARED_REGIONS; i++) {
		if (!shared_regions[i].enabled) {
			shared_regions[i].region_id = i;
			shared_regions[i].partition1 = partition1;
			shared_regions[i].partition2 = partition2;
			shared_regions[i].start_addr =
				0x80000000 +
				(i * size); /* Assign virtual address */
			shared_regions[i].size = size;
			shared_regions[i].access = access;
			shared_regions[i].enabled = 1;
			region_count++;
			*region_id = i;
			return HV_OK;
		}
	}

	return HV_ENOSPC;
}

hv_status_t hv_freertos_get_shared_address(hv_u32 region_id, hv_u32 partition,
					   hv_u64 *address)
{
	if (region_id >= HV_MAX_SHARED_REGIONS || address == NULL) {
		return HV_EINVAL;
	}

	if (!shared_regions[region_id].enabled) {
		return HV_EPERM;
	}

	/* Check partition access. */
	if (partition != shared_regions[region_id].partition1 &&
	    partition != shared_regions[region_id].partition2) {
		return HV_EPERM;
	}

	*address = shared_regions[region_id].start_addr;

	return HV_OK;
}

hv_status_t hv_freertos_deliver_timer(hv_u32 partition)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	freertos_partitions[partition].timer_ticks++;

	return HV_OK;
}

hv_status_t hv_freertos_route_irq(hv_u32 partition, hv_u32 irq)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS || irq >= 256) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	/**
   * In real implementation, would:
   * 1. Check IRQ not already routed
   * 2. Configure interrupt controller to route to partition
   * 3. Track IRQ ownership
   */

	return HV_OK;
}

hv_status_t hv_freertos_unroute_irq(hv_u32 partition, hv_u32 irq)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS || irq >= 256) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	/**
   * In real implementation, would:
   * 1. Check IRQ is routed to partition
   * 2. Remove interrupt routing
   * 3. Clear IRQ ownership
   */

	return HV_OK;
}

hv_status_t hv_freertos_destroy_partition(hv_u32 partition)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	/* Remove all tasks for this partition. */
	for (hv_u32 i = 0; i < HV_MAX_FREERTOS_TASKS_PER_PARTITION *
				       HV_MAX_FREERTOS_PARTITIONS;
	     i++) {
		if (task_registry[i].allocated &&
		    task_registry[i].partition == partition) {
			memset(&task_registry[i], 0, sizeof(task_registry[0]));
			task_count--;
		}
	}

	/* Remove all shared regions involving this partition. */
	for (hv_u32 i = 0; i < HV_MAX_SHARED_REGIONS; i++) {
		if (shared_regions[i].enabled &&
		    (shared_regions[i].partition1 == partition ||
		     shared_regions[i].partition2 == partition)) {
			memset(&shared_regions[i], 0,
			       sizeof(shared_regions[0]));
			region_count--;
		}
	}

	/* Destroy partition. */
	memset(&freertos_partitions[partition], 0,
	       sizeof(freertos_partitions[0]));
	partition_count--;

	return HV_OK;
}

hv_status_t hv_freertos_get_stats(hv_u32 partition, hv_u32 *task_count_ptr,
				  hv_u64 *context_switches, hv_u64 *timer_ticks)
{
	if (partition >= HV_MAX_FREERTOS_PARTITIONS) {
		return HV_EINVAL;
	}

	if (task_count_ptr == NULL || context_switches == NULL ||
	    timer_ticks == NULL) {
		return HV_EINVAL;
	}

	if (!freertos_partitions[partition].allocated) {
		return HV_EPERM;
	}

	*task_count_ptr = freertos_partitions[partition].task_count;
	*context_switches = freertos_partitions[partition].context_switches;
	*timer_ticks = freertos_partitions[partition].timer_ticks;

	return HV_OK;
}
