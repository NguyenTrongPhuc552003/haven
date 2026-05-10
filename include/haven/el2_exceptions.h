/**
 * EL2 (Hypervisor) exception handling interface.
 *
 * Provides exception table configuration, exception handling hooks,
 * and interrupt/exception routing for the hypervisor privilege level (EL2).
 *
 * Exception types:
 *   - Synchronous: SVC/HVC calls, data abort, instruction abort, system errors
 *   - Asynchronous: IRQ (maskable), FIQ (fast interrupt)
 *   - SError: System error exceptions (SEI events on ARM)
 *
 * @file include/haven/el2_exceptions.h
 */

#ifndef HAVEN_EL2_EXCEPTIONS_H
#define HAVEN_EL2_EXCEPTIONS_H

#include <haven/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Exception types (ARM64 architecture).
 */
typedef enum {
	HV_EXC_SYNC = 0,     /**< Synchronous exception (SVC, data/inst abort, etc.) */
	HV_EXC_IRQ = 1,      /**< Interrupt Request (maskable) */
	HV_EXC_FIQ = 2,      /**< Fast Interrupt Request */
	HV_EXC_SERROR = 3,   /**< System Error exception */
} hv_exc_type_t;

/**
 * Exception context saved on EL2 entry.
 */
typedef struct {
	hv_u64 pc;           /**< Program counter (ELR_EL2) */
	hv_u64 sp;           /**< Stack pointer */
	hv_u64 x0, x1, x2, x3, x4, x5, x6, x7;
	hv_u64 x8, x9, x10, x11, x12, x13, x14, x15;
	hv_u64 x16, x17, x18, x19, x20, x21, x22, x23;
	hv_u64 x24, x25, x26, x27, x28, x29, x30;
	hv_u64 spsr;         /**< Saved Program Status Register */
	hv_u64 esr;          /**< Exception Syndrome Register */
	hv_u64 far;          /**< Fault Address Register (for faults) */
	hv_u32 source_partition;  /**< Partition that triggered exception */
} hv_exc_context_t;

/**
 * Exception handler function prototype.
 *
 * Called when exception occurs. Handler must return:
 *   - HV_OK to continue execution (resume guest)
 *   - HV_EPERM for permission denied (reject guest operation)
 *   - HV_EINVAL for invalid operation
 */
typedef hv_status_t (*hv_exc_handler_t)(hv_exc_context_t *ctx);

/**
 * Initialize EL2 exception handling subsystem.
 *
 * Sets up exception table base address, configures exception routing,
 * and registers default handlers.
 *
 * @return HV_OK on success
 */
hv_status_t hv_el2_exceptions_init(void);

/**
 * Register exception handler for specific exception type.
 *
 * Handlers are called when exception occurs. One handler per exception type.
 * Default handlers remain active if no custom handler registered.
 *
 * @param exc_type  Exception type (SYNC, IRQ, FIQ, SERROR)
 * @param handler   Handler function (or NULL to restore default)
 *
 * @return HV_OK on success
 * @return HV_EINVAL if exc_type invalid
 */
hv_status_t hv_el2_register_handler(hv_exc_type_t exc_type, hv_exc_handler_t handler);

/**
 * Register partition-specific exception handler.
 *
 * Allows per-partition customization of exception routing.
 * If no partition-specific handler, default handler used.
 *
 * @param partition Partition ID
 * @param exc_type  Exception type
 * @param handler   Handler function (or NULL to clear)
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition or exc_type invalid
 * @return HV_ENOSPC if max partition-specific handlers reached
 */
hv_status_t hv_el2_register_partition_handler(
	hv_u32 partition,
	hv_exc_type_t exc_type,
	hv_exc_handler_t handler
);

/**
 * Route interrupt to specific partition.
 *
 * Configures interrupt routing so that IRQs/FIQs are delivered to
 * the target partition's exception handler.
 *
 * @param irq       Hardware IRQ number
 * @param partition Target partition ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if irq or partition invalid
 * @return HV_EPERM if IRQ already routed to different partition
 */
hv_status_t hv_el2_route_irq(hv_u32 irq, hv_u32 partition);

/**
 * Unroute interrupt from partition.
 *
 * Removes partition-specific routing, exception falls back to default handler.
 *
 * @param irq       Hardware IRQ number
 *
 * @return HV_OK on success
 * @return HV_EINVAL if irq invalid
 * @return HV_EPERM if IRQ not routed
 */
hv_status_t hv_el2_unroute_irq(hv_u32 irq);

/**
 * Get exception context for current exception.
 *
 * Returns saved exception context (registers, ESR, FAR, etc.)
 * for inspection by handler.
 *
 * @return Pointer to exception context (valid during handler execution)
 */
hv_exc_context_t *hv_el2_get_context(void);

/**
 * Inject exception into guest.
 *
 * Injects a synchronous or asynchronous exception into a guest partition.
 * Used for exception forwarding or guest exception injection.
 *
 * @param partition Target partition
 * @param exc_type  Exception type to inject
 * @param vector    Exception vector index (0-31 for ARM64)
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition, exc_type, or vector invalid
 * @return HV_EPERM if partition not ready or exception injection prohibited
 */
hv_status_t hv_el2_inject_exception(
	hv_u32 partition,
	hv_exc_type_t exc_type,
	hv_u32 vector
);

/**
 * Enable/disable exception type.
 *
 * Temporarily mask or unmask exception delivery for testing/debugging.
 *
 * @param exc_type Exception type
 * @param enable   1 to enable, 0 to disable
 *
 * @return HV_OK on success
 * @return HV_EINVAL if exc_type invalid
 */
hv_status_t hv_el2_set_exception_enabled(hv_exc_type_t exc_type, int enable);

/**
 * Query exception statistics.
 *
 * Returns count of exceptions by type for monitoring/debugging.
 *
 * @param exc_type Exception type to query
 *
 * @return Count of exceptions of specified type, or -1 on error
 */
hv_u64 hv_el2_get_exception_count(hv_exc_type_t exc_type);

#ifdef __cplusplus
}
#endif

#endif /* HAVEN_EL2_EXCEPTIONS_H */
