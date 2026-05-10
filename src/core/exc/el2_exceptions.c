/**
 * EL2 exception handling implementation.
 *
 * @file src/core/exc/el2_exceptions.c
 */

#include <haven/el2_exceptions.h>
#include <string.h>

/**
 * Maximum interrupt routing entries.
 */
#define HV_MAX_IRQ_ROUTES 256

/**
 * Maximum partition-specific handlers.
 */
#define HV_MAX_PARTITION_HANDLERS 1024

/**
 * Global exception context.
 */
static hv_exc_context_t current_exc_context = {0};

/**
 * Global exception handlers (one per exception type).
 */
static hv_exc_handler_t global_handlers[4] = {NULL, NULL, NULL, NULL};

/**
 * Exception enabled flags.
 */
static int exception_enabled[4] = {1, 1, 1, 1};

/**
 * Exception statistics counters.
 */
static hv_u64 exception_counts[4] = {0, 0, 0, 0};

/**
 * IRQ routing table: maps IRQ number to partition ID.
 */
static struct {
	hv_u32 partition;
	int configured;
} irq_routes[HV_MAX_IRQ_ROUTES] = {0};

/**
 * Partition-specific handler table.
 */
static struct {
	hv_u32 partition;
	hv_exc_type_t exc_type;
	hv_exc_handler_t handler;
	int configured;
} partition_handlers[HV_MAX_PARTITION_HANDLERS] = {0};

/**
 * Partition handler count.
 */
static hv_u16 partition_handler_count = 0;

hv_status_t hv_el2_exceptions_init(void) {
	/**
	 * Initialize exception handling state.
	 * In real implementation, would configure VBAR_EL2 (exception table base)
	 * and enable exception delivery.
	 */
	memset(&current_exc_context, 0, sizeof(current_exc_context));
	memset(global_handlers, 0, sizeof(global_handlers));
	memset(irq_routes, 0, sizeof(irq_routes));
	memset(partition_handlers, 0, sizeof(partition_handlers));
	memset(exception_counts, 0, sizeof(exception_counts));

	for (int i = 0; i < 4; i++) {
		exception_enabled[i] = 1;
	}

	return HV_OK;
}

hv_status_t hv_el2_register_handler(hv_exc_type_t exc_type, hv_exc_handler_t handler) {
	if (exc_type < 0 || exc_type >= 4) {
		return HV_EINVAL;
	}

	global_handlers[exc_type] = handler;
	return HV_OK;
}

hv_status_t hv_el2_register_partition_handler(
	hv_u32 partition,
	hv_exc_type_t exc_type,
	hv_exc_handler_t handler
) {
	if (partition >= 256 || exc_type < 0 || exc_type >= 4) {
		return HV_EINVAL;
	}

	/* Search for existing partition handler. */
	for (hv_u16 i = 0; i < partition_handler_count; i++) {
		if (partition_handlers[i].partition == partition &&
			partition_handlers[i].exc_type == exc_type) {
			/* Update existing handler. */
			partition_handlers[i].handler = handler;
			partition_handlers[i].configured = (handler != NULL);
			return HV_OK;
		}
	}

	/* Add new partition handler if space available. */
	if (handler != NULL) {
		if (partition_handler_count >= HV_MAX_PARTITION_HANDLERS) {
			return HV_ENOSPC;
		}

		partition_handlers[partition_handler_count].partition = partition;
		partition_handlers[partition_handler_count].exc_type = exc_type;
		partition_handlers[partition_handler_count].handler = handler;
		partition_handlers[partition_handler_count].configured = 1;
		partition_handler_count++;
	}

	return HV_OK;
}

hv_status_t hv_el2_route_irq(hv_u32 irq, hv_u32 partition) {
	if (irq >= HV_MAX_IRQ_ROUTES || partition >= 256) {
		return HV_EINVAL;
	}

	/* Check if IRQ already routed to different partition. */
	if (irq_routes[irq].configured && irq_routes[irq].partition != partition) {
		return HV_EPERM;
	}

	irq_routes[irq].partition = partition;
	irq_routes[irq].configured = 1;

	return HV_OK;
}

hv_status_t hv_el2_unroute_irq(hv_u32 irq) {
	if (irq >= HV_MAX_IRQ_ROUTES) {
		return HV_EINVAL;
	}

	if (!irq_routes[irq].configured) {
		return HV_EPERM;
	}

	memset(&irq_routes[irq], 0, sizeof(irq_routes[0]));

	return HV_OK;
}

hv_exc_context_t *hv_el2_get_context(void) {
	return &current_exc_context;
}

hv_status_t hv_el2_inject_exception(
	hv_u32 partition,
	hv_exc_type_t exc_type,
	hv_u32 vector
) {
	if (partition >= 256 || exc_type < 0 || exc_type >= 4 || vector >= 32) {
		return HV_EINVAL;
	}

	/**
	 * In real implementation, would:
	 * 1. Validate partition is ready
	 * 2. Write exception injection state to appropriate register
	 * 3. Return from hypervisor to guest with exception pending
	 */

	return HV_OK;
}

hv_status_t hv_el2_set_exception_enabled(hv_exc_type_t exc_type, int enable) {
	if (exc_type < 0 || exc_type >= 4) {
		return HV_EINVAL;
	}

	exception_enabled[exc_type] = enable ? 1 : 0;

	return HV_OK;
}

hv_u64 hv_el2_get_exception_count(hv_exc_type_t exc_type) {
	if (exc_type < 0 || exc_type >= 4) {
		return (hv_u64)-1;
	}

	return exception_counts[exc_type];
}
