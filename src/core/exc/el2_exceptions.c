/**
 * EL2 exception handling implementation.
 *
 * @file src/core/exc/el2_exceptions.c
 */

#include <haven/el2_exceptions.h>
#include <haven/string.h>
#ifdef HAVEN_ARCH_ARM64
#include <haven/platform.h>
#endif

/* External: ARM64 arch layer - weak so host unit-test builds link cleanly. */
extern void hv_install_vectors(void) __attribute__((weak));

/* External: printk for stage-2 fault reporting */
extern void hv_printk(const char *fmt, ...) __attribute__((weak));

/* -----------------------------------------------------------------------
 * ESR_EL2 Exception Class (EC) field: bits [31:26]
 * Only the lower-EL classes that indicate a stage-2 fault are handled.
 * ----------------------------------------------------------------------- */
#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK 0x3FU
#define ESR_EC_IABT_LOW 0x20U /* Instruction Abort from lower EL */
#define ESR_EC_DABT_LOW 0x24U /* Data Abort from lower EL          */
#define ESR_FSC_MASK 0x3FU /* Fault Status Code: bits [5:0] */

/* DFSC / IFSC field: bits [5:0] — fault status code.
 * Stage-2 (and stage-1) faults share the same FSC encoding:
 *   0x04–0x07 = Translation fault, level 0–3
 *   0x08–0x0B = Access flag fault, level 0–3
 *   0x0C–0x0F = Permission fault, level 0–3
 * S1PTW (ESR_EL2 bit [7] for DABT) = 0 for direct guest access (stage-2),
 *                                   = 1 for stage-1 page table walk fault.
 * We catch the whole range and skip the instruction for any stage-2 path. */
#define ESR_FSC_S2_FIRST 0x04U /* first fault code (translation level 0) */
#define ESR_FSC_S2_LAST 0x0FU /* last  fault code (permission level 3)  */
#define ESR_ISS_S1PTW (1U << 7) /* stage-1 page table walk flag */

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

hv_status_t hv_el2_exceptions_init(void)
{
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

hv_status_t hv_el2_register_handler(hv_exc_type_t exc_type,
				    hv_exc_handler_t handler)
{
	if (exc_type < 0 || exc_type >= 4) {
		return HV_EINVAL;
	}

	global_handlers[exc_type] = handler;
	return HV_OK;
}

hv_status_t hv_el2_register_partition_handler(hv_u32 partition,
					      hv_exc_type_t exc_type,
					      hv_exc_handler_t handler)
{
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

		partition_handlers[partition_handler_count].partition =
			partition;
		partition_handlers[partition_handler_count].exc_type = exc_type;
		partition_handlers[partition_handler_count].handler = handler;
		partition_handlers[partition_handler_count].configured = 1;
		partition_handler_count++;
	}

	return HV_OK;
}

hv_status_t hv_el2_route_irq(hv_u32 irq, hv_u32 partition)
{
	if (irq >= HV_MAX_IRQ_ROUTES || partition >= 256) {
		return HV_EINVAL;
	}

	/* Check if IRQ already routed to different partition. */
	if (irq_routes[irq].configured &&
	    irq_routes[irq].partition != partition) {
		return HV_EPERM;
	}

	irq_routes[irq].partition = partition;
	irq_routes[irq].configured = 1;

	return HV_OK;
}

hv_status_t hv_el2_unroute_irq(hv_u32 irq)
{
	if (irq >= HV_MAX_IRQ_ROUTES) {
		return HV_EINVAL;
	}

	if (!irq_routes[irq].configured) {
		return HV_EPERM;
	}

	memset(&irq_routes[irq], 0, sizeof(irq_routes[0]));

	return HV_OK;
}

hv_exc_context_t *hv_el2_get_context(void)
{
	return &current_exc_context;
}

hv_status_t hv_el2_inject_exception(hv_u32 partition, hv_exc_type_t exc_type,
				    hv_u32 vector)
{
	if (partition >= 256 || exc_type < 0 || exc_type >= 4 || vector >= 32) {
		return HV_EINVAL;
	}

	if (!exception_enabled[exc_type]) {
		return HV_EPERM;
	}

	/**
   * In real implementation, would:
   * 1. Validate partition is ready
   * 2. Write exception injection state to appropriate register
   * 3. Return from hypervisor to guest with exception pending
   */

	exception_counts[exc_type]++;
	return HV_OK;
}

hv_status_t hv_el2_set_exception_enabled(hv_exc_type_t exc_type, int enable)
{
	if (exc_type < 0 || exc_type >= 4) {
		return HV_EINVAL;
	}

	exception_enabled[exc_type] = enable ? 1 : 0;

	return HV_OK;
}

hv_u64 hv_el2_get_exception_count(hv_exc_type_t exc_type)
{
	if (exc_type < 0 || exc_type >= 4) {
		return (hv_u64)-1;
	}

	return exception_counts[exc_type];
}

/* -----------------------------------------------------------------------
 * Assembly entry points - called directly from arch/arm64/entry.S.
 *
 * The assembly stubs save minimal registers and call these C functions.
 * esr: Exception Syndrome Register (ESR_EL2)
 * far: Fault Address Register (FAR_EL2)
 * sp:  EL2 stack pointer at time of exception
 * ----------------------------------------------------------------------- */

void hv_handle_sync(hv_u64 esr, hv_u64 far, hv_u64 sp)
{
	current_exc_context.esr = esr;
	current_exc_context.far = far;
	current_exc_context.sp = sp;
	exception_counts[HV_EXC_SYNC]++;

#ifdef HAVEN_ARCH_ARM64
	{
		hv_u32 ec = (hv_u32)((esr >> ESR_EC_SHIFT) & ESR_EC_MASK);
		hv_u32 fsc = (hv_u32)(esr & ESR_FSC_MASK);

		/* Stage-2 fault from a lower EL guest: log the violation and skip.
		 * S1PTW=0 means this is a direct access (not a stage-1 PTW fault).
		 * HPFAR_EL2 holds the faulting IPA (bits[47:4] → IPA[51:8]).
		 * We advance the saved ELR in the stack frame by 4 so that
		 * restore_minimal + ERET skips the faulting instruction. */
		if ((ec == ESR_EC_IABT_LOW || ec == ESR_EC_DABT_LOW) &&
		    !(esr & (hv_u64)ESR_ISS_S1PTW) && fsc >= ESR_FSC_S2_FIRST &&
		    fsc <= ESR_FSC_S2_LAST) {
			hv_u64 hpfar;
			hv_u64 ipa;

			__asm__ volatile("mrs %0, hpfar_el2" : "=r"(hpfar));
			/* HPFAR_EL2 bits[47:4] hold IPA[51:8]; shift left 8 */
			ipa = (hpfar & ~0xFULL) << 8;

			if (hv_printk)
				hv_printk("HAVEN: Denied stage-2 violation "
					  "IPA=0x%lx FAR=0x%lx ESR=0x%lx\n",
					  (unsigned long)ipa,
					  (unsigned long)far,
					  (unsigned long)esr);

			/* Update saved ELR at frame+168 (see save_minimal layout) */
			{
				volatile hv_u64 *saved_elr =
					(volatile hv_u64 *)(sp + 168U);
				*saved_elr += 4ULL;
			}
			return;
		}
	}
#endif

	if (global_handlers[HV_EXC_SYNC])
		global_handlers[HV_EXC_SYNC](&current_exc_context);
}

void hv_handle_irq(hv_u64 esr, hv_u64 far, hv_u64 sp)
{
	current_exc_context.esr = esr;
	current_exc_context.sp = sp;
	(void)far;
	exception_counts[HV_EXC_IRQ]++;

	if (global_handlers[HV_EXC_IRQ])
		global_handlers[HV_EXC_IRQ](&current_exc_context);
}

void hv_handle_fiq(hv_u64 esr, hv_u64 far, hv_u64 sp)
{
	current_exc_context.esr = esr;
	current_exc_context.sp = sp;
	(void)far;
	exception_counts[HV_EXC_FIQ]++;

	if (global_handlers[HV_EXC_FIQ])
		global_handlers[HV_EXC_FIQ](&current_exc_context);
}

void hv_handle_serror(hv_u64 esr, hv_u64 far, hv_u64 sp)
{
	current_exc_context.esr = esr;
	current_exc_context.far = far;
	current_exc_context.sp = sp;
	exception_counts[HV_EXC_SERROR]++;

	if (global_handlers[HV_EXC_SERROR])
		global_handlers[HV_EXC_SERROR](&current_exc_context);
}

/* Called for unrecoverable EL2 faults (e.g., synchronous abort at EL2).
 * Prints diagnostics and halts; never returns. */
void __attribute__((noreturn)) hv_fatal_exception(hv_u64 esr, hv_u64 far,
						  hv_u64 elr, hv_u64 sp)
{
	(void)esr;
	(void)far;
	(void)elr;
	(void)sp;
#ifdef HAVEN_ARCH_ARM64
	platform_uart_putchar('!');
	platform_uart_putchar('E');
	platform_uart_putchar('L');
	platform_uart_putchar('2');
	platform_uart_putchar('\n');
	for (;;)
		__asm__ volatile("wfi");
#else
	for (;;) {
	}
#endif
}
