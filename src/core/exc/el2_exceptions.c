/**
 * EL2 exception handling implementation.
 *
 * @file src/core/exc/el2_exceptions.c
 */

#include <haven/el2_exceptions.h>
#include <haven/mmio.h>
#include <haven/psci.h>
#include <haven/vgic.h>
#include <haven/string.h>
#include <haven/sysreg_trap.h>
#ifdef HAVEN_ARCH_ARM64
#include <haven/platform.h>
#endif

/* External: ARM64 arch layer - weak so host unit-test builds link cleanly. */
extern void hv_install_vectors(void) __attribute__((weak));
extern void hv_arch_gic_el2_setup(void) __attribute__((weak));
#ifdef HAVEN_ARCH_ARM64
extern uint32_t hv_arch_cpu_id(void);
#endif
extern void hv_arch_handle_phys_irq(void) __attribute__((weak));
extern hv_status_t hv_arch_inject_virtual_irq(uint32_t virq, uint32_t priority)
	__attribute__((weak));
extern void hv_arch_irq_report(uint32_t cpu) __attribute__((weak));

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
#define ESR_EC_HVC64 0x16U /* HVC from AArch64; ELR = next insn */
#define ESR_EC_SMC64 0x17U /* SMC trapped by HCR_EL2.TSC; ELR = the SMC */
#define ESR_EC_SYSREG 0x18U /* MSR/MRS trapped; ELR = the instruction */
#define ESR_FSC_MASK 0x3FU /* Fault Status Code: bits [5:0] */

/* DFSC / IFSC field: bits [5:0] - fault status code.
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
	memset(&current_exc_context, 0, sizeof(current_exc_context));
	memset(global_handlers, 0, sizeof(global_handlers));
	memset(irq_routes, 0, sizeof(irq_routes));
	memset(partition_handlers, 0, sizeof(partition_handlers));
	memset(exception_counts, 0, sizeof(exception_counts));

	for (int i = 0; i < 4; i++) {
		exception_enabled[i] = 1;
	}

#ifdef HAVEN_ARCH_ARM64
	/* Install the ARM64 exception vector table (sets VBAR_EL2).
	 * hv_install_vectors() is defined in arch/arm64/cpu.c and uses
	 * the .el2_exception_table symbol from arch/arm64/entry.S. */
	if (hv_install_vectors != NULL)
		hv_install_vectors();

	/* Configure the GICv3 virtual CPU interface so the hypervisor can
	 * inject virtual interrupts into guests via ICH_LR<n>_EL2. */
	if (hv_arch_gic_el2_setup != NULL)
		hv_arch_gic_el2_setup();
#endif

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

#ifdef HAVEN_ARCH_ARM64
	/*
	 * IRQ injection: write a GICv3 List Register (ICH_LR<n>_EL2) with
	 * the virtual IRQ number and Pending state.  The guest sees the
	 * interrupt when it takes an IRQ exception from EL1 (lower EL).
	 *
	 * For SYNC/FIQ/SERR types the hardware path is not yet wired
	 * (requires ESR/FAR injection which is software-model only); fall
	 * through to the counter update so the policy layer tracks them.
	 */
	if (exc_type == HV_EXC_IRQ && hv_arch_inject_virtual_irq != NULL) {
		hv_status_t rc = hv_arch_inject_virtual_irq(
			(uint32_t)vector, HV_VIRT_IRQ_PRIORITY);
		if (rc != HV_OK)
			return rc;
	}
#endif

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

/* Classify a synchronous exception from a lower EL (EX2/E3).
 * A direct stage-2 abort is EC 0x20/0x24 with S1PTW = 0 and FSC 0x04-0x0F.
 * Data aborts are skipped (ELR += 4) as before. Instruction aborts stop the
 * partition: ELR is the address that could not be fetched, so skipping to
 * ELR + 4 would fault again on the next instruction of the same page. */
/* Denied stage-2 accesses per CPU (log rate limit; Linux chain case set
 * v1, T4: a guest using ungranted RAM caused 101,905 log lines in 45 s). */
#define HV_DENIED_CPUS 16U
static hv_u64 denied_count[HV_DENIED_CPUS];

int hv_denied_note(hv_u32 cpu)
{
	hv_u64 n;

	if (cpu >= HV_DENIED_CPUS)
		return 1;
	n = ++denied_count[cpu];
	return n <= HV_DENIED_LOG_FIRST || (n % HV_DENIED_LOG_EVERY) == 0U;
}

hv_u64 hv_denied_count(hv_u32 cpu)
{
	return cpu < HV_DENIED_CPUS ? denied_count[cpu] : 0U;
}

hv_s2_fault_action_t hv_s2_fault_action(hv_u64 esr)
{
	hv_u32 ec = (hv_u32)((esr >> ESR_EC_SHIFT) & ESR_EC_MASK);
	hv_u32 fsc = (hv_u32)(esr & ESR_FSC_MASK);

	if (ec != ESR_EC_IABT_LOW && ec != ESR_EC_DABT_LOW) {
		return HV_S2F_NOT_S2;
	}
	if ((esr & (hv_u64)ESR_ISS_S1PTW) != 0U) {
		return HV_S2F_NOT_S2;
	}
	if (fsc < ESR_FSC_S2_FIRST || fsc > ESR_FSC_S2_LAST) {
		return HV_S2F_NOT_S2;
	}

	return (ec == ESR_EC_IABT_LOW) ? HV_S2F_STOP : HV_S2F_SKIP;
}

#ifdef HAVEN_ARCH_ARM64
/* PSCI over HVC, or over SMC trapped by HCR_EL2.TSC. The caller's x0..x3
 * are at frame+0..24 (save_minimal layout); the result goes back in x0.
 * A trapped SMC returns to the SMC itself, so ELR is moved past it. */
/* Multi-vCPU partitions (src/core/partition.c, multi-vcpu.md): the vCPU
 * table of the partition on a CPU, starting one of its vCPUs, and stopping
 * its other vCPUs before it stops. Absent or NULL: one vCPU. */
extern struct hv_psci_vcpus *hv_partition_vcpus(hv_u32 cpu)
	__attribute__((weak));
extern int hv_partition_start_vcpu(hv_u32 index, hv_u64 entry, hv_u64 context)
	__attribute__((weak));
extern void hv_partition_stop_vcpus(hv_u32 cpu) __attribute__((weak));
extern void hv_partition_vcpu_off(hv_u32 cpu) __attribute__((weak));
extern void hv_spin_lock(volatile uint32_t *lock);
extern void hv_spin_unlock(volatile uint32_t *lock);
static volatile uint32_t psci_vcpus_lock;

static void hv_handle_psci_call(hv_u32 ec, hv_u64 sp)
{
	volatile hv_u64 *frame = (volatile hv_u64 *)sp;
	hv_u64 x[4] = {frame[0], frame[1], frame[2], frame[3]};
	hv_u64 mpidr;
	hv_u64 ret = 0;
	hv_psci_action_t action;
	hv_u32 cpu = hv_arch_cpu_id();
	struct hv_psci_vcpus *v =
		hv_partition_vcpus != NULL ? hv_partition_vcpus(cpu) : NULL;
	hv_u32 index = 0U;

	__asm__ volatile("mrs %0, vmpidr_el2" : "=r"(mpidr));
	if (v != NULL) {
		hv_spin_lock(&psci_vcpus_lock);
		action = hv_psci_call_vcpus(x, mpidr, &ret, v, &index);
		hv_spin_unlock(&psci_vcpus_lock);
	} else {
		action = hv_psci_call(x, mpidr, &ret);
	}
	if (action == HV_PSCI_START_VCPU) {
		/*
		 * Outside the lock: the start waits for the CPU. Only the
		 * vCPU table path asks for a start; the check keeps a start
		 * without a table from writing through NULL.
		 */
		int rc = v != NULL && hv_partition_start_vcpu != NULL ?
				 hv_partition_start_vcpu(index, x[2], x[3]) :
				 HV_PSCI_INTERNAL_FAILURE;

		if (v != NULL) {
			hv_spin_lock(&psci_vcpus_lock);
			v->state[index] = rc == 0 ? HV_VCPU_ON : HV_VCPU_OFF;
			hv_spin_unlock(&psci_vcpus_lock);
		}
		ret = (hv_u64)(int64_t)rc;
		action = HV_PSCI_RESUME;
	}

	if (hv_printk)
		hv_printk("HAVEN: PSCI %s fid=0x%lx x1=0x%lx -> 0x%lx cpu=%u\n",
			  hv_psci_name((hv_u32)x[0]), (unsigned long)x[0],
			  (unsigned long)x[1],
			  (unsigned long)(action == HV_PSCI_RESUME ? ret : 0U),
			  (unsigned)hv_arch_cpu_id());

	if (action == HV_PSCI_STOP_VCPU) {
		/* One vCPU of several switched itself off (SMP3): its CPU is
		 * switched off through the firmware and can be started again
		 * by a later CPU_ON; the partition's other vCPUs keep running. */
		if (hv_arch_irq_report != NULL)
			hv_arch_irq_report(cpu);
		if (hv_printk)
			hv_printk(
				"HAVEN: vCPU %u stopped by PSCI CPU_OFF; CPU %u "
				"switched off\n",
				(unsigned)index, (unsigned)cpu);
		if (hv_partition_vcpu_off != NULL)
			hv_partition_vcpu_off(cpu);
		if (hv_printk)
			hv_printk("HAVEN: CPU %u not switched off; parked\n",
				  (unsigned)cpu);
		for (;;) {
			__asm__ volatile("wfe");
		}
	}
	if (action != HV_PSCI_RESUME) {
		/* A partition with several vCPUs stops the others first; each
		 * logs its own report before this CPU logs its own. */
		if (v != NULL && hv_partition_stop_vcpus != NULL)
			hv_partition_stop_vcpus(cpu);
		/* Stop the partition: park this CPU at EL2 with exceptions
		 * masked, as for a stage-2 instruction abort. A restart for
		 * SYSTEM_RESET is not implemented yet. */
		if (hv_arch_irq_report != NULL)
			hv_arch_irq_report(hv_arch_cpu_id());
		if (hv_printk && hv_denied_count(hv_arch_cpu_id()) != 0U)
			hv_printk("HAVEN: %lu denied stage-2 accesses on CPU "
				  "%u\n",
				  (unsigned long)hv_denied_count(
					  hv_arch_cpu_id()),
				  (unsigned)hv_arch_cpu_id());
		if (hv_printk)
			hv_printk("HAVEN: partition stopped by PSCI %s; "
				  "CPU parked\n",
				  action == HV_PSCI_STOP_RESET ?
					  "SYSTEM_RESET" :
					  "OFF");
		for (;;) {
			__asm__ volatile("wfe");
		}
	}

	frame[0] = ret;
	if (ec == ESR_EC_SMC64)
		frame[21] += 4ULL; /* saved ELR at frame+168 */
}
#endif

void hv_handle_sync(hv_u64 esr, hv_u64 far, hv_u64 sp)
{
	current_exc_context.esr = esr;
	current_exc_context.far = far;
	current_exc_context.sp = sp;
	exception_counts[HV_EXC_SYNC]++;

#ifdef HAVEN_ARCH_ARM64
	{
		hv_u32 ec = (hv_u32)((esr >> ESR_EC_SHIFT) & ESR_EC_MASK);

		if (ec == ESR_EC_HVC64 || ec == ESR_EC_SMC64) {
			hv_handle_psci_call(ec, sp);
			return;
		}
		if (ec == ESR_EC_SYSREG) {
			hv_handle_sysreg_trap(esr, (hv_u64 *)sp,
					      hv_arch_cpu_id());
			return;
		}
	}
	{
		hv_s2_fault_action_t action = hv_s2_fault_action(esr);

		/* Direct stage-2 abort from a guest: log it, then either skip the
		 * instruction (data abort) or stop the partition (instruction
		 * abort). HPFAR_EL2 bits[47:4] hold IPA[51:8]; for a direct
		 * permission fault the value may be UNKNOWN (FINDINGS EX1), so
		 * judge permission faults by ESR and FAR. */
		if (action != HV_S2F_NOT_S2) {
			hv_u64 hpfar;
			hv_u64 ipa;
			hv_mmio_access_t acc;
			int decoded = -1;
			int logged;

			__asm__ volatile("mrs %0, hpfar_el2" : "=r"(hpfar));
			ipa = (hpfar & ~0xFULL) << 8;

			/* Data abort on an unmapped page (translation fault, so
			 * HPFAR is valid): emulate it if a device model is
			 * registered for this address (L2/L3). */
			if (action == HV_S2F_SKIP) {
				decoded = hv_mmio_decode(esr, &acc);
				if (decoded == 0 && (esr & 0x3CU) == 0x04U &&
				    hv_mmio_emulate(hv_arch_cpu_id(),
						    ipa | (far & 0xFFFU), &acc,
						    (hv_u64 *)sp) == 0) {
					((volatile hv_u64 *)sp)[HV_FRAME_ELR] +=
						4ULL;
					return;
				}
			}

			logged = hv_denied_note(hv_arch_cpu_id());
			if (logged && hv_printk)
				hv_printk("HAVEN: Denied stage-2 violation "
					  "IPA=0x%lx FAR=0x%lx ESR=0x%lx "
					  "cpu=%u n=%lu\n",
					  (unsigned long)ipa,
					  (unsigned long)far,
					  (unsigned long)esr,
					  (unsigned)hv_arch_cpu_id(),
					  (unsigned long)hv_denied_count(
						  hv_arch_cpu_id()));

			if (action == HV_S2F_STOP) {
				/* Minimal stop: never return to the guest; park this
				 * CPU at EL2 with exceptions still masked. Other CPUs
				 * and partitions are not affected. */
				if (hv_printk)
					hv_printk(
						"HAVEN: partition stopped after "
						"stage-2 instruction abort; "
						"CPU parked\n");
				for (;;) {
					__asm__ volatile("wfe");
				}
			}

			/* Denied access with a decodable syndrome: a load reads
			 * as zero, a store is ignored (RAZ/WI). */
			if (decoded == 0) {
				if (logged && hv_printk)
					hv_printk(
						"HAVEN: decoded: %s %u bytes, "
						"reg %c%u, IPA=0x%lx; %s\n",
						acc.write ? "write" : "read",
						(unsigned)acc.size,
						acc.reg64 ? 'x' : 'w',
						(unsigned)acc.reg,
						(unsigned long)(ipa |
								(far & 0xFFFU)),
						acc.write ? "ignored" :
							    "reads as zero");
				if (!acc.write)
					hv_frame_set((hv_u64 *)sp, acc.reg, 0U);
			}

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

	if (global_handlers[HV_EXC_IRQ]) {
		global_handlers[HV_EXC_IRQ](&current_exc_context);
		return;
	}
#ifdef HAVEN_ARCH_ARM64
	if (hv_arch_handle_phys_irq != NULL)
		hv_arch_handle_phys_irq();
#endif
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
	/* Preserve the fault context in the UART log before halting. */
	if (hv_printk != NULL) {
		hv_printk("EL2 FATAL: ESR=0x%lx FAR=0x%lx ELR=0x%lx\n",
			  (unsigned long)esr, (unsigned long)far,
			  (unsigned long)elr);
	}
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
