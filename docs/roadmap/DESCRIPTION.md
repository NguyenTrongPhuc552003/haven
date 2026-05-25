# Haven: One-Year Master's Thesis Implementation Plan

## Context

Haven is an EL2 static-partition hypervisor for heterogeneous AMP SoCs (Cortex-A + Cortex-M/R). The research question: *Can a minimal EL2 hypervisor enforce hard spatial and temporal isolation between Linux and RTOS on a real SoC, while remaining small enough for safety-certification analysis?*

The project has solid contract-level C (7 isolation modules, ~1,200 lines, all tested), strong CI/CD infrastructure (~3,500 lines of tests, multi-compiler, cross-platform), and excellent documentation scaffolding - but is critically missing its **hardware binding layer** (no ARM64 assembly, no EL2 page-table programming, no GIC/SMMU register access). Over 15 directories are `.gitkeep` placeholders. The 12-month plan below converts this framework into a publishable, thesis-grade implementation.

---

## Part 1: Reference Architecture Analysis

### Why Linux Kernel Structure Matters

The Linux kernel's organizational philosophy produces the world's most peer-reviewed embedded codebase. Haven should adopt its key structural lessons:

| Linux Kernel Pattern                                | Problem It Solves                                     | Haven Application                                                                |
| --------------------------------------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------- |
| `arch/<target>/` - all HW-specific code isolated    | Prevents hardware details leaking into portable logic | `src/core/arch/arm64/` for entry.S, context.S, hw registers                      |
| `drivers/<subsystem>/` - drivers separate from core | Subsystem policy ≠ hardware driver                    | `drivers/irqchip/` for GICv3, `drivers/iommu/` for SMMUv3                        |
| `include/linux/` vs `include/uapi/`                 | Public API vs internal API                            | `include/haven/` (public) vs `include/arch/` (hardware)                          |
| `lib/` - shared utilities (string, bitmap, etc.)    | Avoids code duplication                               | `src/common/` with string.c, printk.c, panic.c                                   |
| `Documentation/` with subdirectories                | Living docs tied to subsystems                        | Expand `docs/` with subsystem-specific guides                                    |
| CMake ≥ 3.22 - presets, Ninja, cross-compile        | Fast, reproducible builds                             | CMake is the primary build system; `CMakePresets.json` defines arm64-qemu, arm64-imx95, host-tests |

### Comparable Projects: Key Structural Lessons

**Jailhouse (Siemens)** - most architecturally similar:
- `hypervisor/arch/arm64/` - exception entry, CPU setup, MMU, GIC, SMMU
- `hypervisor/pci.c`, `hypervisor/mmio.c` - device isolation at hypervisor level
- `configs/<board>/` - static cell configs (YAML/JSON)
- `inmates/` - demo guests showing the isolation model in action
- `driver/` - Linux kernel module (.ko) bridging host OS to hypervisor
- Lesson: Clean arch/ separation; a working Linux kernel module proves guest integration

**Bao Hypervisor (University of Minho)** - ARM64-first static hypervisor:
- `src/arch/aarch64/` - entry, armv8_a.c, smmu.c, gic.c, timer.c
- `src/core/` - platform-neutral policy (vm.c, sched.c, mem.c, iompc.c)
- `src/platform/<board>/` - board init per SoC
- Lesson: Driver code lives in arch/, platform init in platform/; core stays portable

**seL4** - formal verification reference:
- `src/arch/arm/` - ARM-specific
- `src/kernel/` - verified kernel core
- `proof/` - Isabelle/HOL proofs organized by theorem
- `tools/` - verification toolchain
- Lesson: Separation between verified core (minimal, auditable) and unverified drivers is the thesis TCB story

**Omnivisor (ECRTS 2024)** - mixed-criticality reference for evaluation methodology:
- Evaluates worst-case latency under interference on COTS ARM SoCs
- Compares static vs dynamic partitioning overhead
- Lesson: Temporal isolation claims need measured WCET + jitter under adversarial Linux load; not just code-level contracts

---

## Part 2: Current State Assessment

### Implementation Completeness

| Module               | File                                    | Status                   | Gap                                          |
| -------------------- | --------------------------------------- | ------------------------ | -------------------------------------------- |
| Stage-2 MMU          | `src/core/mm/stage2.c`                  | Contract-complete        | No ARM64 TTBR0_EL2/page-table hardware       |
| IRQ Ownership        | `src/core/irq/ownership.c`              | Contract-complete        | No GICv3 GICD/GICR register writes           |
| Budget Scheduler     | `src/core/sched/budget.c`               | Contract-complete        | No timer interrupt driver, no hardware clock |
| SMMU DMA             | `src/core/dma/smmu.c`                   | Contract-complete        | No SMMUv3 stream table writes                |
| IOMMU Policy         | `src/core/iommu/iommu_policy.c`         | Contract-complete        | No hardware binding                          |
| Timer                | `src/core/time/timer.c`                 | Contract-complete        | No CNTHP_CTL_EL2 / CNTVOFF programming       |
| EL2 Exceptions       | `src/core/exc/el2_exceptions.c`         | Partial - injection stub | No VBAR_EL2, no exception vector table       |
| UART Driver          | `src/guest/drivers/uart.c`              | Partial - flush stub     | No MMIO register access                      |
| FreeRTOS Integration | `src/guest/rtos/freertos_integration.c` | Partial - context stub   | No ARM64 GP register save/restore            |

### Empty Directories (15 `.gitkeep` placeholders)

```
src/core/arch/        ← Most critical gap: no ARM64 assembly
src/common/           ← No utilities (printk, panic, string ops)
src/platform/         ← No board init
include/arch/         ← No hardware register headers
drivers/guest-tools/  ← No userspace CLI for haven
drivers/linux/        ← No Linux kernel module
configs/riscv/        ← No RISC-V configs
configs/x86/          ← No x86 configs
docs/contributing/    ← Contributing guides missing
tools/analysis/       ← No data analysis scripts
tools/scripts/        ← No cross-compile helpers
verification/coq/     ← No Coq proofs
verification/isabelle/ ← No Isabelle proofs
tests/demos/          ← (now has demo_two_partition.c - no longer empty)
tests/selftests/      ← (now has test_hypervisor_invariants.c - no longer empty)
```

---

## Part 3: Proposed Repository Restructuring

### Target Layout (Linux/Bao-inspired)

```
haven/
├── arch/
│   └── arm64/                    ← [NEW] All ARM64-specific code
│       ├── entry.S               ← EL2 exception vector table
│       ├── context.S             ← GP register save/restore
│       ├── cpu.c                 ← HCR_EL2, SCTLR_EL2, MDCR_EL2 init
│       ├── mm.c                  ← TTBR0_EL2, page-table walk
│       ├── timer.c               ← CNTHP_CTL_EL2, CNTVOFF_EL2
│       └── include/
│           └── asm/
│               ├── sysregs.h     ← ARM64 system register accessors
│               ├── page.h        ← Stage-2 page descriptor formats
│               ├── gic.h         ← GICv3 register offsets
│               └── smmu.h        ← SMMUv3 register offsets
│
├── drivers/
│   ├── irqchip/
│   │   ├── gic_v3.c             ← [NEW] GICv3 programming
│   │   └── gic_v3.h
│   ├── iommu/
│   │   ├── smmu_v3.c            ← [NEW] SMMUv3 stream tables
│   │   └── smmu_v3.h
│   ├── uart/
│   │   ├── pl011.c              ← [NEW] PL011 (QEMU)
│   │   ├── imx_uart.c           ← [NEW] i.MX UART
│   │   └── uart.h
│   ├── linux/
│   │   ├── haven_driver.c       ← [NEW] Linux .ko guest bridge
│   │   ├── Kbuild
│   │   └── README.md
│   └── guest-tools/
│       ├── haven_tool.c         ← [NEW] Userspace CLI
│       └── haven_ioctl.h        ← IOCTL interface
│
├── include/
│   ├── haven/                   ← (keep existing public headers)
│   └── arch/                    ← Redirect to arch/arm64/include/asm/
│
├── src/
│   ├── core/                    ← Platform-neutral isolation policy (keep)
│   │   ├── mm/stage2.c          ← Calls arch/ for hardware writes
│   │   ├── irq/ownership.c      ← Calls drivers/irqchip/ for GIC writes
│   │   ├── sched/budget.c       ← Calls arch/arm64/timer.c for clock
│   │   ├── dma/smmu.c           ← Calls drivers/iommu/ for SMMU writes
│   │   ├── iommu/iommu_policy.c ← (keep)
│   │   ├── time/timer.c         ← Calls arch/ timer abstraction
│   │   ├── exc/el2_exceptions.c ← Calls arch/arm64/entry.S dispatch
│   │   └── init.c               ← [NEW] Hypervisor init sequence
│   ├── common/                  ← [NEW] Utility functions (no .gitkeep)
│   │   ├── printk.c
│   │   ├── string.c
│   │   ├── panic.c
│   │   └── spinlock.c
│   ├── guest/                   ← (keep)
│   └── platform/                ← [NEW] Board-specific init
│       ├── qemu-virt/
│       │   ├── platform.c       ← UART/GIC/memory base addresses
│       │   └── platform.h
│       ├── imx95-devkit/
│       │   ├── platform.c
│       │   └── platform.h
│       └── imx8qm-mek/
│           ├── platform.c
│           └── platform.h
│
├── configs/
│   ├── arm64/                   ← (keep YAML files)
│   ├── riscv/
│   │   ├── qemu-riscv64.yaml    ← [NEW] RISC-V future target
│   │   └── README.md
│   └── x86/
│       ├── qemu-x86_64.yaml     ← [NEW] x86 CI baseline
│       └── README.md
│
├── tests/                       ← (keep existing test structure)
│   ├── unit/
│   ├── integration/
│   ├── isolation/
│   ├── benchmarks/
│   ├── selftests/
│   └── demos/
│
├── verification/
│   ├── coq/
│   │   ├── IsolationModel.v     ← [NEW] Core isolation invariants
│   │   ├── Stage2Policy.v       ← [NEW] Stage-2 policy proofs
│   │   ├── BudgetScheduler.v    ← [NEW] Budget invariants
│   │   └── README.md
│   ├── isabelle/
│   │   ├── HavenIsolation.thy   ← [NEW] Isabelle theory (optional depth)
│   │   └── README.md
│   └── README.md
│
├── tools/
│   ├── analysis/
│   │   ├── latency_analyzer.py  ← [NEW] Parse benchmark JSON → stats
│   │   ├── jitter_plot.py       ← [NEW] Matplotlib jitter plots
│   │   ├── evidence_report.py   ← [NEW] HTML/PDF evidence report
│   │   └── README.md
│   ├── config-gen/
│   │   └── README.md            ← (expand - config validation tool)
│   └── scripts/
│       ├── cross-compile.sh     ← [NEW] Cross-compile environment setup
│       ├── qemu-run.sh          ← [NEW] QEMU launch for integration tests
│       └── README.md
│
├── docs/
│   ├── architecture/            ← (keep + expand)
│   ├── contributing/            ← [NEW] Development, testing, review guides
│   │   ├── DEVELOPMENT_GUIDE.md
│   │   ├── TESTING_GUIDE.md
│   │   └── REVIEW_CHECKLIST.md
│   ├── methodology/             ← (keep)
│   ├── porting/                 ← (keep + complete runbooks)
│   ├── roadmap/                 ← (keep + expand monthly milestones)
│   ├── safety/                  ← (keep)
│   └── thesis/                  ← (keep + fill evidence)
│
├── website/                     ← (keep Astro Starlight site)
├── scripts/                     ← (keep existing scripts)
├── CMakeLists.txt               ← PRIMARY build system (replaces Makefile)
├── CMakePresets.json            ← arm64-qemu, arm64-imx95, arm64-imx8qm, host-tests
├── cmake/                       ← arm64.cmake, host.cmake, flags.cmake
└── .github/                     ← (keep + expand workflows)
```

---

## Part 4: Twelve-Month Implementation Plan

### Release 1 (Month 1–3): Architecture Layer + Foundation Hardening

**Theme:** Transform empty `.gitkeep` directories into a real ARM64 architecture layer. Establish the hardware binding scaffold without which all thesis claims remain theoretical.

#### Month 1: ARM64 Assembly Foundation

**Week 1–2: System Register Infrastructure**

*Create: `arch/arm64/include/asm/sysregs.h`*
- `read_sysreg(r)` / `write_sysreg(v, r)` macros using GCC `__asm__`
- All EL2 control registers: `HCR_EL2`, `VTTBR_EL2`, `TTBR0_EL2`, `VTCR_EL2`
- All timer registers: `CNTHP_CTL_EL2`, `CNTHP_TVAL_EL2`, `CNTPCT_EL0`, `CNTVOFF_EL2`
- GICv3 system registers: `ICC_SRE_EL2`, `ICH_HCR_EL2`, `ICC_CTLR_EL1`
- SMMU system registers: SMMUv3 MMIO base + SMMU_IDR0/IDR1/CR0/CR1/CR2

*Create: `arch/arm64/include/asm/page.h`*
- Stage-2 descriptor format (ARMv8.x VMSAv8-64)
- `PGD_TYPE_TABLE`, `PMD_TYPE_SECT`, `PTE_TYPE_PAGE` - stage-2 specific
- Memory attributes: `S2_MEMATTR_NORMAL_WB`, `S2_MEMATTR_DEVICE_nGnRE`
- Access permissions: `S2AP_RO`, `S2AP_WO`, `S2AP_RW`
- 4K, 2M, 1G granule support

*Create: `arch/arm64/include/asm/gic.h`*
- GICv3 MMIO offsets: `GICD_CTLR`, `GICD_IROUTER<n>`, `GICD_ISENABLER<n>`
- GICv3 redistributor: `GICR_CTLR`, `GICR_WAKER`, `GICR_ISENABLER0`
- GICR stride and GICR base calculation
- `GICD_IROUTER_AFF` encoding macros

*Create: `arch/arm64/include/asm/smmu.h`*
- SMMUv3 MMIO register offsets: `SMMU_IDR0..5`, `SMMU_CR0`, `SMMU_STRTAB_BASE`
- Stream table entry format (STE Level-1/Level-2)
- Context descriptor (CD) format
- `SMMU_STRTAB_SPLIT`, `SMMU_STE_VALID`, `SMMU_STE_TYPE_CD_S1`

**Week 3–4: EL2 Exception Vectors**

*Create: `arch/arm64/entry.S`* - the most critical missing file
```
/* EL2 exception table aligned to 2KB (VBAR_EL2 requirement) */
.align 11
.global el2_exception_table
el2_exception_table:
    /* Current EL with SP0 */
    vector_entry sync_invalid_el2t
    vector_entry irq_invalid_el2t
    ...
    /* Current EL with SP_ELx (EL2 running at EL2) */
    vector_entry sync_el2_sp2
    vector_entry irq_el2_sp2
    ...
    /* Lower EL (EL1/EL0 → guest → trap to EL2) */
    vector_entry sync_lower_el64      ← main guest trap handler
    vector_entry irq_lower_el64       ← guest IRQ
    vector_entry fiq_lower_el64       ← guest FIQ  
    vector_entry serror_lower_el64    ← SError
    ...
```
- Each vector entry saves minimal registers, calls C dispatch function
- Must save/restore: x0-x30, ELR_EL2, SPSR_EL2, SP_EL1

*Create: `arch/arm64/context.S`*
```asm
/* hv_save_guest_context(struct hv_context *ctx) */
/* hv_restore_guest_context(struct hv_context *ctx) */
/* Save: x0-x30, SP_EL0, SP_EL1, ELR_EL1, SPSR_EL1 */
/* Save system registers: SCTLR_EL1, TTBR0_EL1, TTBR1_EL1, MAIR_EL1 */
/* NEON/FP registers: Q0-Q31, FPCR, FPSR */
```

*Update: `include/haven/el2_exceptions.h`* - add `struct hv_context` for context save

#### Month 2: CPU Init + Stage-2 Hardware Binding

**Week 5–6: EL2 CPU Initialization**

*Create: `arch/arm64/cpu.c`*
- `hv_arch_cpu_init()` - programs EL2 control registers:
  - `HCR_EL2`: enable TGE=0, VM=1, IMO=1, FMO=1, AMO=1, RW=1 (AArch64 guests)
  - `VPIDR_EL2`, `VMPIDR_EL2`: virtual MPIDR for guests
  - `MDCR_EL2`: debug/performance monitor trap configuration
  - `HSTR_EL2`: system register trap policy
  - `CPTR_EL2`: SVE/NEON trap policy
- `hv_arch_el2_enter()` - transition from EL3/firmware to EL2

*Create: `src/core/init.c`* - hypervisor entry point (fills critical gap)
```c
void haven_init(uintptr_t dtb_pa, uintptr_t hyp_load_pa) {
    hv_arch_cpu_init();           // ARM64: HCR_EL2, VTTBR_EL2
    hv_stage2_init();             // Policy: partition tables
    hv_irq_owner_init();          // Policy: IRQ map  
    hv_budget_sched_init();       // Policy: budget state
    hv_smmu_init();               // Policy: SMMU config
    hv_iommu_init();              // Policy: IOMMU groups
    hv_timer_init();              // Policy: timer state
    hv_el2_exceptions_init();     // Policy: exception handlers
    platform_init();              // Board: UART, GIC, SMMU base addrs
    config_load(dtb_pa);          // Load partition config from DTB
    partitions_launch();          // Start guest partitions
}
```

**Week 7–8: Stage-2 Page Table Hardware**

*Create: `arch/arm64/mm.c`* - fills the biggest hardware gap
```c
/* Programs VTTBR_EL2, VTCR_EL2, MAIR_EL2 */
int hv_arch_stage2_map(struct hv_stage2_state *state,
                       uint64_t ipa, uint64_t pa, uint64_t size,
                       uint32_t flags);
int hv_arch_stage2_unmap(struct hv_stage2_state *state,
                         uint64_t ipa, uint64_t size);
void hv_arch_stage2_flush_tlb(uint32_t vmid);
void hv_arch_stage2_enable(uint64_t ttbr0, uint32_t vmid);
```
- 3-level page table (39-bit IPA space, 4K granule)
- Descriptor allocation from static pool (no malloc)
- TLB invalidation: `TLBI VMALLS12E1IS` broadcast

*Update: `src/core/mm/stage2.c`* - call `hv_arch_stage2_map()` after policy validation

#### Month 3: Platform Abstraction + Common Utilities

**Week 9–10: Common Utilities (fill `src/common/`)**

*Create: `src/common/printk.c`*
- `hv_printk(const char *fmt, ...)` - vsnprintf-based, calls platform UART
- No dynamic allocation; output goes to early UART

*Create: `src/common/string.c`*
- `hv_memset()`, `hv_memcpy()`, `hv_strlen()`, `hv_strcmp()`
- All bounds-safe, no reliance on libc

*Create: `src/common/panic.c`*
- `hv_panic(const char *msg)` - prints message, halts all CPUs via `WFI` loop

*Create: `src/common/spinlock.c`*
- `hv_spin_lock()` / `hv_spin_unlock()` using ARMv8 `LDAXR`/`STLXR`
- Required for SMP safety when CPUs share isolation state

**Week 11–12: Platform Board Abstraction (fill `src/platform/`)**

*Create: `src/platform/platform.h`* - common platform interface
```c
struct hv_platform_ops {
    void (*uart_putchar)(char c);
    uint64_t (*gic_base)(void);
    uint64_t (*smmu_base)(void);
    uint64_t (*timer_freq)(void);
};
extern const struct hv_platform_ops *platform;
```

*Create: `src/platform/qemu-virt/platform.c`*
- UART: PL011 at `0x09000000`
- GIC: GICv3 distributor at `0x08000000`, redistributors at `0x080A0000`
- SMMU: `0x08080000` (QEMU virt SMMUv3)
- Timer frequency: `0x62FEFAC`

*Create: `src/platform/imx95-devkit/platform.c`*
- i.MX95 UART base, GIC base, SMMUv3 base from i.MX95 Reference Manual
- A55 CPU frequency, timer calibration
- Board-specific power sequence notes

*Create: `src/platform/imx8qm-mek/platform.c`*
- Secondary validation platform
- i.MX8QM GIC400 (GICv2), IOMMU differences documented

**Month 3 Exit Criteria:**
- [ ] `cmake --preset arm64-qemu && cmake --build build` completes without error
- [ ] `arch/arm64/entry.S` assembles with GNU assembler
- [ ] `src/core/init.c` compiles and calls all `_init()` functions
- [ ] All existing unit tests still pass (regression gate)
- [ ] `include/arch/` no longer has `.gitkeep` - real headers present
- [ ] `src/common/` no longer has `.gitkeep` - 4 utility files present
- [ ] `src/platform/` no longer has `.gitkeep` - 3 board directories present

---

### Release 2 (Month 4–6): Hardware Drivers + GIC/SMMU Integration

**Theme:** Real hardware drivers. By end of Month 6, the isolation mechanisms connect to actual GICv3 and SMMUv3 hardware registers. The isolation model is no longer theoretical.

#### Month 4: GICv3 Interrupt Controller Driver

**Week 13–14: GICv3 Distributor + Redistributor**

*Create: `drivers/irqchip/gic_v3.c`*
- `gic_v3_init(uintptr_t gicd_base, uintptr_t gicr_base, int nr_cpus)`
- `gic_v3_configure_irq(uint32_t irq, uint8_t priority, uint64_t affinity)`
- `gic_v3_enable_irq(uint32_t irq)`
- `gic_v3_disable_irq(uint32_t irq)` - used for partition isolation
- `gic_v3_route_irq(uint32_t irq, uint64_t mpidr)` - programs GICD_IROUTER
- `gic_v3_ack_irq(void)` → `iar = read_sysreg(ICC_IAR1_EL1)`
- `gic_v3_eoi_irq(uint32_t iar)` → `write_sysreg(iar, ICC_EOIR1_EL1)`

*Update: `src/core/irq/ownership.c`*
- `hv_irq_assign()` now calls `gic_v3_route_irq(irq, target_cpu_mpidr)`
- `hv_irq_revoke()` now calls `gic_v3_disable_irq(irq)`
- IRQ ownership table is now enforced at the hardware level, not just software policy

**Week 15–16: EL2 Interrupt Handling + Virtualization**

*Create: `arch/arm64/irq.c`*
- `hv_arch_irq_enable()` / `hv_arch_irq_disable()` - DAIF flags
- `hv_arch_gic_el2_setup()` - ICH_HCR_EL2, ICH_VMCR_EL2
- `hv_arch_inject_virtual_irq(uint32_t virq)` - programs ICH_LR0_EL2 (List Register)
- `hv_arch_handle_guest_irq_exit()` - determine which list register fired

*Complete: `src/core/exc/el2_exceptions.c:hv_el2_inject_exception()`*
- Now uses `hv_arch_inject_virtual_irq()` to write List Registers
- Real IRQ injection to guest through GICv2/v3 virtualization interface
- Removes the last major STUB from the codebase

#### Month 5: SMMUv3 Stream Table Driver

**Week 17–18: SMMUv3 Initialization**

*Create: `drivers/iommu/smmu_v3.c`*
```c
/* SMMUv3 hardware initialization */
int smmu_v3_init(uintptr_t base, struct smmu_v3_config *cfg);

/* Linear Stream Table (type 1) setup */
int smmu_v3_alloc_stream_table(struct smmu_v3 *smmu, uint32_t nr_entries);

/* Stream Table Entry (STE) programming */
int smmu_v3_set_ste_bypass(struct smmu_v3 *smmu, uint32_t sid);
int smmu_v3_set_ste_partition(struct smmu_v3 *smmu, uint32_t sid,
                               uint64_t vttbr, uint32_t vmid);
int smmu_v3_set_ste_abort(struct smmu_v3 *smmu, uint32_t sid);

/* SMMU_CR0: enable/disable */
void smmu_v3_enable(struct smmu_v3 *smmu);

/* TLB invalidation after STE update */
void smmu_v3_tlbi_ste(struct smmu_v3 *smmu, uint32_t sid);
```

Key STE fields to implement:
- `STE.VALID=1`, `STE.TYPE=0b01` (CD, stage-2-only)
- `STE.S2TTB` - VTTBR for this stream's stage-2 table
- `STE.S2VMID` - partition VMID
- `STE.S2AA64=1`, `STE.S2TF0` (4KB granule, 40-bit PA)
- Abort STE: `STE.VALID=1, STE.TYPE=0b000` → faults all DMA

**Week 19–20: SMMU Integration with Core**

*Update: `src/core/dma/smmu.c`*
- `hv_smmu_configure_dma_window()` now calls `smmu_v3_set_ste_partition()`
- `hv_smmu_revoke_dma_access()` now calls `smmu_v3_set_ste_abort()`
- `hv_smmu_init()` calls `smmu_v3_init()` with platform base address

*Create: `tests/integration/test_smmu_hardware.c`* (new test, QEMU-targeted)
- Tests that DMA from unassigned StreamID triggers SMMU fault
- Tests that DMA within assigned window succeeds
- Tests that DMA to PA outside assigned window triggers stage-2 fault

#### Month 6: Stage-2 Table Hardware Integration + Build System

**Week 21–22: Stage-2 Full Hardware Path**

*Integrate `src/core/mm/stage2.c` → `arch/arm64/mm.c`*:
- `hv_stage2_map_partition()` now calls `hv_arch_stage2_map()` for each region
- VMID assigned per partition (up to 256 VMIDs on ARMv8.0)
- `VTTBR_EL2` programmed per partition context switch
- TLB flush on partition teardown

*Create: `arch/arm64/timer.c`*
- `hv_arch_timer_init()` - CNTHP_CTL_EL2 = 1 (enable EL2 physical timer)
- `hv_arch_timer_set_deadline(uint64_t cval)` → `write_sysreg(cval, CNTHP_CVAL_EL2)`
- `hv_arch_timer_now()` → `read_sysreg(CNTPCT_EL0)` (EL2 physical counter)
- `hv_arch_timer_freq()` → `read_sysreg(CNTFRQ_EL0)`

*Update: `src/core/time/timer.c`*
- `hv_timer_check_deadline()` calls `hv_arch_timer_now()` for real timestamps
- `hv_timer_set_deadline()` programs CNTHP_CVAL_EL2

**Week 23–24: Improved Build System**

*Update: `CMakeLists.txt` and `cmake/`* - cross-compile support complete via CMake presets
```makefile
ARCH       ?= arm64
CROSS_COMPILE ?= aarch64-unknown-linux-gnu-
CC         = $(CROSS_COMPILE)gcc
AS         = $(CROSS_COMPILE)gcc
LD         = $(CROSS_COMPILE)ld
OBJCOPY    = $(CROSS_COMPILE)objcopy

CFLAGS     = -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
             -nostdlib -fno-stack-protector \
             -Iinclude -Iarch/$(ARCH)/include \
             -DHAVEN_ARCH_$(shell echo $(ARCH) | tr a-z A-Z)

ASFLAGS    = -Iarch/$(ARCH)/include

# Targets
all: build/haven.elf build/haven.bin

build/haven.elf: $(OBJS)
    $(LD) -T linker.ld -o $@ $^

build/haven.bin: build/haven.elf
    $(OBJCOPY) -O binary $< $@
```

*Create: `linker.ld`* - ELF linker script
- Places `.text` at `0x80000000` (EL2 load address for QEMU virt)
- `.rodata`, `.data`, `.bss` sections
- Stack setup for 4 CPUs (4KB each)

*Create: `tools/scripts/cross-compile.sh`*
- Detects host OS (macOS/Linux/Windows-WSL)
- Installs/verifies `aarch64-unknown-linux-gnu-gcc`
- Tests cross-compilation with `arm64/cpu.c`

**Month 6 Exit Criteria:**
- [ ] `cmake --preset arm64-qemu && cmake --build build` produces `build/haven.elf` (real ARM64 binary)
- [ ] `drivers/irqchip/gic_v3.c` compiles cleanly with ARM64 toolchain
- [ ] `drivers/iommu/smmu_v3.c` compiles cleanly
- [ ] `src/core/irq/ownership.c` + `gic_v3.c` linked together - IRQ assignment writes hardware
- [ ] `src/core/dma/smmu.c` + `smmu_v3.c` linked - DMA policy writes hardware
- [ ] `drivers/` directory: no `.gitkeep` files, 3 real driver subsystems

---

### Release 3 (Month 7–9): QEMU Execution + i.MX95 Bring-Up

**Theme:** The hypervisor must boot and run on real platforms. This phase produces the thesis's primary evidence: measured isolation on QEMU (reproducible CI) and i.MX95 (hardware ground truth).

#### Month 7: QEMU End-to-End Boot

**Week 25–26: QEMU Boot Chain**

*Create: `scripts/qemu-run.sh`* (upgrade from current qemu-smoke.sh)
```bash
#!/bin/bash
# Launch haven hypervisor on QEMU virt board
qemu-system-aarch64 \
  -machine virt,virtualization=on,gic-version=3 \
  -cpu cortex-a72 \
  -smp 4 \
  -m 2G \
  -nographic \
  -bios ./build/haven.bin \
  -device virtio-net-pci \
  -kernel ${LINUX_IMAGE} \
  -append "console=ttyAMA0 root=/dev/vda" \
  -drive if=none,file=${ROOTFS},format=qcow2,id=hd0 \
  -device virtio-blk-pci,drive=hd0 \
  -S -gdb tcp::1234  # optional: GDB stub
```

*Create: `arch/arm64/boot.S`*
- Primary CPU initialization at EL2 entry
- Secondary CPU bring-up via `PSCI_CPU_ON`
- SP stack setup from BSS
- Jumps to `haven_init()`

*Create: `src/platform/qemu-virt/memory.h`*
- QEMU virt memory map
- PCIE, VIRTIO, SMMU, GIC base addresses
- Haven load address and guest memory regions

**Week 27–28: First Two-Partition Boot**

*Create: `src/core/partition.c`* - partition launcher
```c
void partitions_launch(void) {
    // Load partition configs from qemu-virt.yaml (baked into binary or DTB)
    // For each partition:
    //   hv_stage2_map_partition(id, ipa_base, pa_base, size, flags)
    //   hv_irq_assign(irq, id, cpu)
    //   hv_budget_set(id, period_ns, budget_ns)
    //   Start CPU in guest context via hv_arch_start_partition()
}
```

*Create: `arch/arm64/partition.S`*
- `hv_arch_start_partition(uintptr_t entry, uintptr_t sp, uintptr_t arg)`
- Sets up SPSR_EL2 for AArch64 EL1, ELR_EL2 = entry point
- Sets up stage-2 translation: write VTTBR_EL2 with partition VMID
- ERET to guest entry

Goal: Boot QEMU with Haven loading a minimal bare-metal "guest" that:
1. Prints "Hello from Partition A" via emulated UART
2. Tries to access Partition B's memory → triggers stage-2 fault → Haven prints "ACCESS DENIED"
3. Exits cleanly

**Month 7 Artifacts:**
- `haven.bin` runs on `qemu-system-aarch64`
- UART output shows partition isolation in action
- Screenshot/log captured and archived in `build/evidence/qemu/`

#### Month 8: i.MX95 Hardware Bring-Up

**Week 29–30: i.MX95 Boot Chain**

*Create: `src/platform/imx95-devkit/memory.h`*
- i.MX95 DDR memory map (2GB at 0x80000000)
- Cortex-A55 cluster base addresses
- Cortex-M7 ITCM/DTCM addresses (0x00000000 / 0x20000000)
- i.MX95 UART1 base (0x44380000)
- i.MX95 GIC base (from A55 APB)

*Create: `docs/porting/IMX95_VALIDATION_RUNBOOK.md`* (critical missing doc)
Step-by-step:
1. Build: `cmake --preset arm64-imx95 && cmake --build build-imx95`
2. Flash: `uuu -b emmc_all imx-boot.bin haven.bin`
3. Connect: `screen /dev/ttyUSB0 115200`
4. Expected output: Haven initialization messages
5. Evidence capture: `script -q -c "..." build/evidence/imx95/run-$(date +%Y%m%d).log`

**Week 31–32: i.MX95 Isolation Validation**

Goal: Reproduce the QEMU two-partition demo on real hardware:
1. Linux partition boots on A55[0–3]
2. RTOS stub on A55[4] (simulating M7 until Cortex-M7 integration)
3. Haven enforces stage-2 on DRAM access
4. Evidence log shows: partition init, negative access denied, budget enforced

*Create: `docs/porting/IMX95_EVIDENCE_CAPTURE.md`*
- Automated evidence extraction script
- JSON schema for structured evidence: `{ platform, timestamp, test_name, result, log_hash }`

*Update: `scripts/package-evidence.sh`*
- Ingest i.MX95 logs alongside QEMU logs
- Cross-compare outputs for consistency

#### Month 9: Cross-Platform CI + Secondary Board

**Week 33–34: Linux Kernel Module (fills `drivers/linux/`)**

*Create: `drivers/linux/haven_driver.c`*
- Linux 6.x kernel module (`MODULE_LICENSE("GPL")`)
- Exposes `/dev/haven` character device
- IOCTL: `HAVEN_IOCTL_GET_PARTITION_INFO`, `HAVEN_IOCTL_TRIGGER_TEST`
- Reads partition state via MMIO hypercall (`HVC #0`)
- Used by host Linux to inspect hypervisor state from inside a partition

*Create: `drivers/linux/Kbuild`* - standard Linux kernel build file

**Week 35–36: Tools and Analysis (fills `tools/analysis/`)**

*Create: `tools/analysis/latency_analyzer.py`*
- Parses `build/benchmarks/isolation-latency.json` (already produced by bench_isolation_latency.c)
- Computes: mean, p99, p99.9, max latency per enforcement operation
- Outputs comparison table suitable for thesis Chapter 5

*Create: `tools/analysis/jitter_plot.py`*
- Matplotlib histogram of latency distributions
- Overlays QEMU vs i.MX95 measurements
- Saves PNG for thesis figures

*Create: `tools/analysis/evidence_report.py`*
- Generates HTML report from all evidence JSON
- Includes: platform metadata, test results, latency graphs, traceability links

**Month 9 Exit Criteria:**
- [ ] `haven.bin` boots on QEMU with 2-partition demo
- [ ] `haven.bin` boots on i.MX95 with isolation evidence captured
- [ ] Evidence logs archived with SHA256 checksums
- [ ] `drivers/linux/haven_driver.c` compiles against kernel headers
- [ ] `tools/analysis/` has 3 working Python scripts
- [ ] CI `cross-os.yml` validates QEMU boot on Ubuntu/macOS/Windows

---

### Release 4 (Month 10–12): Formal Verification + Thesis Lock

**Theme:** Formal proofs for the isolation invariants, publication-quality benchmark data, thesis submission packaging.

#### Month 10: Formal Verification

**Week 37–38: Coq Isolation Model**

*Create: `verification/coq/IsolationModel.v`*
```coq
(* Core isolation invariant: no PA overlap between partitions *)
Theorem spatial_isolation :
  forall (s : HvState) (p1 p2 : PartitionId) (pa : PhysAddr),
    p1 <> p2 ->
    partition_contains_pa s p1 pa ->
    ~partition_contains_pa s p2 pa.
```

*Create: `verification/coq/Stage2Policy.v`*
```coq
(* Stage-2 map operation preserves no-overlap invariant *)
Lemma stage2_map_preserves_isolation :
  forall s pa ipa size id,
    stage2_invariant s ->
    hv_stage2_map s id ipa pa size = (HV_OK, s') ->
    stage2_invariant s'.
```

*Create: `verification/coq/BudgetScheduler.v`*
```coq
(* Budget never exceeds period *)
Theorem budget_le_period :
  forall s id,
    partition_configured s id ->
    budget s id <= period s id.
```

Key: These proofs are for the *policy* (C-level contracts), not the hardware implementation. This is the correct TCB story: the policy is formally verified; the hardware binding is test-validated.

**Week 39–40: Isabelle Theory (optional depth)**

*Create: `verification/isabelle/HavenIsolation.thy`*
- Isabelle/HOL theory for the same invariants
- Cross-validates the Coq proof style
- Higher prestige for academic reviewers

*Update: `verification/README.md`* - describe proof structure, tools needed (Coq 8.18+, Isabelle 2023)

#### Month 11: Benchmark Campaigns + Thesis Evidence

**Week 41–42: Temporal Isolation Benchmarks**

*Create: `tests/benchmarks/bench_temporal_isolation.c`* - the key thesis benchmark
```c
/* Measures RTOS task response time while Linux is under load */
/* Test matrix:
 *   Linux load: 0%, 25%, 50%, 75%, 100% CPU
 *   RTOS period: 1ms, 5ms, 10ms
 *   Metric: response latency, jitter (p50/p95/p99/max)
 *   Platform: QEMU and i.MX95
 */
```

*Create: `tests/benchmarks/bench_stage2_fault.c`*
- Measures stage-2 fault handler latency (time from illegal access to handler return)
- Baseline: < 2µs on QEMU, target < 5µs on i.MX95

*Create: `tests/benchmarks/bench_smmu_policy.c`*
- Measures SMMU stream table lookup overhead on valid DMA
- Baseline: < 1µs overhead per DMA transaction

*Update: `docs/methodology/BENCHMARK_BASELINE.md`* - fill with actual data
- QEMU latency baselines (absolute numbers from bench runs)
- i.MX95 latency baselines (board-measured)
- Acceptance thresholds: RTOS deadline miss rate < 0.1%

**Week 43–44: Traceability Matrix Completion**

*Update: `docs/methodology/CHAPTER_TRACEABILITY.md`*
- Every chapter row now has: code reference + test file + evidence log path + measured result
- Chapter 4 (Spatial Isolation): links to fault_injection test results + i.MX95 logs
- Chapter 5 (Temporal Isolation): links to bench_temporal_isolation results
- Chapter 6 (Evaluation): links to `build/evidence/` artifacts

*Update: `docs/safety/THREAT_MODEL.md`*
- Every threat scenario now has: test mapping + measured result + residual risk
- F1-F8 faults from test_fault_injection.c linked to threat IDs

#### Month 12: Thesis Submission Packaging

**Week 45–46: Documentation Completion**

*Fill: `docs/contributing/`*
- `DEVELOPMENT_GUIDE.md` - full build environment setup, cross-compile, QEMU
- `TESTING_GUIDE.md` - how to add unit/integration/isolation tests
- `REVIEW_CHECKLIST.md` - isolation guardian checklist, TCB size review

*Update: `docs/architecture/OVERVIEW.md`* - expand from 11 lines to 500+ words covering:
- System architecture diagram (ASCII art)
- 5-layer isolation model
- Hardware binding layer explanation
- Comparison with Jailhouse/Bao

*Update: `docs/architecture/ISOLATION_MODEL.md`* - expand from 16 lines:
- Spatial isolation: stage-2 + SMMU + IOMMU
- Temporal isolation: budget + timer + IRQ
- Formal properties (link to Coq proofs)
- Threat model mapping

**Week 47–48: Final Validation Run + Release Tag**

*Create: `scripts/release.sh`* (complete final version)
- Full validation matrix: unit tests → integration → isolation → benchmarks
- Evidence packaging with SHA256 manifest
- `git tag -s v1.0.0 -m "Thesis submission release"`

*Create: `docs/thesis/REPRODUCIBILITY_APPENDIX.md`*
- Exact commands to reproduce all results from scratch
- Docker image hash for exact toolchain
- Expected outputs with acceptable variance ranges

*Final CI gate: `evidence-pack.yml`*
- Must pass on i.MX95 AND QEMU
- Evidence bundle uploaded as GitHub release artifact
- SHA256 manifest committed to repo

**Month 12 Exit Criteria:**
- [ ] Formal proofs in `verification/coq/` check under Coq 8.18
- [ ] `bench_temporal_isolation` measurements on both QEMU and i.MX95
- [ ] RTOS deadline miss rate < 0.1% under 100% Linux CPU stress
- [ ] Stage-2 fault latency < 5µs on i.MX95
- [ ] All 8 fault injection scenarios (F1-F8) confirmed DENY on real hardware
- [ ] Complete `docs/methodology/CHAPTER_TRACEABILITY.md` with evidence links
- [ ] `verification/coq/` no longer `.gitkeep` - 3 `.v` files with proofs checked
- [ ] Publication-quality plots in `tools/analysis/` output

---

## Part 5: Files Created Per Phase Summary

### Phase 1 (M1–3): 22 new files, 3 empty dirs eliminated

| File                                   | Purpose                         |
| -------------------------------------- | ------------------------------- |
| `arch/arm64/include/asm/sysregs.h`     | ARM64 register accessors        |
| `arch/arm64/include/asm/page.h`        | Stage-2 page descriptor formats |
| `arch/arm64/include/asm/gic.h`         | GICv3 register map              |
| `arch/arm64/include/asm/smmu.h`        | SMMUv3 register map             |
| `arch/arm64/entry.S`                   | EL2 exception vector table      |
| `arch/arm64/context.S`                 | ARM64 context save/restore      |
| `arch/arm64/cpu.c`                     | EL2 CPU initialization          |
| `arch/arm64/mm.c`                      | Stage-2 page table hardware     |
| `arch/arm64/timer.c`                   | EL2 physical timer              |
| `src/core/init.c`                      | Hypervisor entry point          |
| `src/common/printk.c`                  | Hypervisor output               |
| `src/common/string.c`                  | Safe string ops                 |
| `src/common/panic.c`                   | Panic handler                   |
| `src/common/spinlock.c`                | SMP spinlock                    |
| `src/platform/platform.h`              | Platform ops interface          |
| `src/platform/qemu-virt/platform.c`    | QEMU board                      |
| `src/platform/imx95-devkit/platform.c` | i.MX95 board                    |
| `src/platform/imx8qm-mek/platform.c`   | Secondary board                 |
| `linker.ld`                            | ELF linker script               |
| `tools/scripts/cross-compile.sh`       | Cross-compile setup             |
| `configs/riscv/qemu-riscv64.yaml`      | RISC-V future config            |
| `configs/x86/qemu-x86_64.yaml`         | x86 CI config                   |

### Phase 2 (M4–6): 14 new files, 2 empty dirs eliminated

| File                                     | Purpose                    |
| ---------------------------------------- | -------------------------- |
| `drivers/irqchip/gic_v3.c`               | GICv3 interrupt controller |
| `drivers/irqchip/gic_v3.h`               | GICv3 interface            |
| `drivers/iommu/smmu_v3.c`                | SMMUv3 stream tables       |
| `drivers/iommu/smmu_v3.h`                | SMMUv3 interface           |
| `drivers/uart/pl011.c`                   | PL011 UART (QEMU)          |
| `drivers/uart/imx_uart.c`                | i.MX UART                  |
| `arch/arm64/irq.c`                       | EL2 IRQ handling           |
| `arch/arm64/partition.S`                 | Partition launch / ERET    |
| `arch/arm64/boot.S`                      | Primary boot entry         |
| `src/core/partition.c`                   | Partition launcher logic   |
| `tests/integration/test_smmu_hardware.c` | HW SMMU test               |
| `tools/scripts/qemu-run.sh`              | QEMU launch script         |
| `CMakeLists.txt` + `CMakePresets.json` + `cmake/`  | Cross-compile + preset build system |
| `linker.ld` (updated)                    | Final linker script        |

### Phase 3 (M7–9): 12 new files, 4 empty dirs eliminated

| File                                          | Purpose                |
| --------------------------------------------- | ---------------------- |
| `src/platform/imx95-devkit/memory.h`          | i.MX95 memory map      |
| `src/platform/qemu-virt/memory.h`             | QEMU memory map        |
| `drivers/linux/haven_driver.c`                | Linux kernel module    |
| `drivers/linux/Kbuild`                        | Kernel build file      |
| `drivers/guest-tools/haven_tool.c`            | Userspace CLI          |
| `drivers/guest-tools/haven_ioctl.h`           | IOCTL interface        |
| `docs/porting/IMX95_VALIDATION_RUNBOOK.md`    | Board runbook          |
| `docs/porting/IMX95_EVIDENCE_CAPTURE.md`      | Evidence methodology   |
| `tools/analysis/latency_analyzer.py`          | Latency statistics     |
| `tools/analysis/jitter_plot.py`               | Matplotlib plots       |
| `tools/analysis/evidence_report.py`           | HTML evidence report   |
| `tests/benchmarks/bench_temporal_isolation.c` | RTOS latency benchmark |

### Phase 4 (M10–12): 12 new files, 3 empty dirs eliminated

| File                                             | Purpose                |
| ------------------------------------------------ | ---------------------- |
| `verification/coq/IsolationModel.v`              | Core isolation proofs  |
| `verification/coq/Stage2Policy.v`                | Stage-2 policy proof   |
| `verification/coq/BudgetScheduler.v`             | Budget invariant proof |
| `verification/isabelle/HavenIsolation.thy`       | Isabelle theory        |
| `tests/benchmarks/bench_stage2_fault.c`          | Fault handler latency  |
| `tests/benchmarks/bench_smmu_policy.c`           | SMMU overhead          |
| `docs/contributing/DEVELOPMENT_GUIDE.md`         | Dev setup guide        |
| `docs/contributing/TESTING_GUIDE.md`             | Test writing guide     |
| `docs/contributing/REVIEW_CHECKLIST.md`          | Review checklist       |
| `docs/thesis/REPRODUCIBILITY_APPENDIX.md`        | Reproduction guide     |
| `src/platform/imx95-devkit/platform.c` (updated) | Final iMX95 platform   |
| `CMakeLists.txt`                                 | Optional CMake for IDE |

---

## Part 6: Quality Gates and Publication Readiness

### Per-Release Quality Gates

| Gate                              | R1 (M3) | R2 (M6) | R3 (M9) | R4 (M12) |
| --------------------------------- | ------- | ------- | ------- | -------- |
| ARM64 binary builds               | ✓       | ✓       | ✓       | ✓        |
| All unit + integration tests pass | ✓       | ✓       | ✓       | ✓        |
| QEMU boots to isolation demo      |         | ✓       | ✓       | ✓        |
| i.MX95 evidence package           |         |         | ✓       | ✓        |
| Formal proofs check               |         |         |         | ✓        |
| RTOS latency ≤ threshold          |         |         | ✓       | ✓        |
| Zero `.gitkeep` in key dirs       |         | ✓       | ✓       | ✓        |
| Traceability matrix complete      |         |         | ✓       | ✓        |

### Publication Readiness Checklist (ECRTS / DATE / RTSS / EMSOFT target)

For submission-quality paper, Haven must demonstrate:
1. **Novelty claim**: Static EL2 hypervisor with formally-verified policy + measured hardware enforcement - distinct from Jailhouse (no formal proofs), seL4 (dynamic, microkernel), Bao (no formal verification), Omnivisor (dynamic partitioning)
2. **Evaluation rigor**: RTOS latency measurements under 5 Linux stress levels, on 2 platforms (QEMU + i.MX95), with standard statistical reporting (mean/p99/max, error bars, ≥100 runs)
3. **Reproducibility**: Independent rerun from `scripts/qemu-run.sh` + Docker image produces matching results
4. **TCB audit**: TCB lines of count (target < 5,000 lines of core), no dynamic allocation, no FP, bounded loops - provable by `wc -l src/core/` + code review

---

## Part 7: Thesis Chapter → Implementation Mapping

| Chapter                     | Implementation Artifacts                                                                                  | Evidence Artifacts                                            |
| --------------------------- | --------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| Ch1: Introduction           | `README.md`, `docs/architecture/OVERVIEW.md`                                                              | -                                                             |
| Ch2: Background             | `docs/architecture/THESIS_DEEP_DIVE.md`                                                                   | Omnivisor/Jailhouse comparison                                |
| Ch3: Design                 | `include/haven/*.h`, `src/core/init.c`, `src/platform/`                                                   | Architecture diagrams                                         |
| Ch4: Spatial Isolation      | `src/core/mm/`, `src/core/irq/`, `src/core/dma/`, `arch/arm64/mm.c`, `drivers/irqchip/`, `drivers/iommu/` | `tests/isolation/test_spatial_isolation.c`, i.MX95 fault logs |
| Ch5: Temporal Isolation     | `src/core/sched/`, `src/core/time/`, `arch/arm64/timer.c`                                                 | `tests/benchmarks/bench_temporal_isolation.c`, latency plots  |
| Ch6: Formal Verification    | `verification/coq/*.v`, `verification/isabelle/*.thy`                                                     | Coq proof check output                                        |
| Ch7: Evaluation             | `tests/`, `tools/analysis/`                                                                               | Evidence JSON, HTML report, latency tables                    |
| Ch8: Conclusion             | `docs/safety/THREAT_MODEL.md`, `docs/safety/ASSUMPTIONS.md`                                               | Traceability matrix                                           |
| Appendix A: Reproducibility | `docs/thesis/REPRODUCIBILITY_APPENDIX.md`                                                                 | Docker image SHA                                              |
| Appendix B: API Reference   | `include/haven/*.h`, website API docs                                                                     | -                                                             |

---

## Verification Strategy

### End-to-End Test After Each Phase

**Phase 1 (M3):**
```bash
cmake --preset arm64-qemu && cmake --build build
# Expect: build/haven.elf (ARM64 ELF)
ctest --test-dir build-host  # Still passes host-compiled tests
aarch64-unknown-linux-gnu-objdump -d build/haven.elf | grep el2_exception_table
# Expect: exception table at aligned address
```

**Phase 2 (M6):**
```bash
./tools/scripts/qemu-run.sh --kernel build/haven.bin --no-guest
# Expect: "Haven EL2 init OK" on serial
# Expect: "GICv3 init OK: 256 IRQs"
# Expect: "SMMUv3 init OK: stream table at 0x..."
```

**Phase 3 (M9):**
```bash
./scripts/qemu-smoke.sh 2>&1 | tee build/evidence/qemu/smoke-$(date +%Y%m%d).log
# Expect: "PARTITION_A: Hello from Linux-class guest"
# Expect: "PARTITION_B: RTOS task running, period=10ms"
# Expect: "HAVEN: Denied stage-2 violation from partition 0 to PA 0x..."
python3 tools/analysis/evidence_report.py build/evidence/
# Expect: HTML report with pass/fail matrix
```

**Phase 4 (M12):**
```bash
cd verification/coq && coqc IsolationModel.v Stage2Policy.v BudgetScheduler.v
# Expect: No errors - proofs check
python3 tools/analysis/latency_analyzer.py build/benchmarks/temporal-imx95.json
# Expect: p99 < 50µs, max < 100µs for 10ms period RTOS task
```

---

## Part 8: Post-Thesis Extension Roadmap

The sections below describe work beyond the 12-month thesis window. They are captured here to preserve architectural intent and provide a clear handoff for continued development or follow-on research.

---

### Phase 5: Secondary CPU Full Integration + FreeRTOS Wire-Up Validation

**Theme:** Validate the full AMP dual-partition path on real hardware - secondary CPU bring-up confirmed, FreeRTOS context switch mediated at EL2, measured scheduling fidelity.

**Scope:**
- Harden `hv_arch_psci_cpu_on` path: add PSCI error-code logging (`PSCI_E_ALREADY_ON`, `PSCI_E_INVALID_PARAMS`, etc.)
- Wire FreeRTOS `configCPU_CLOCK_HZ` to the EL2 physical timer frequency (`CNTFRQ_EL0`)
- Implement `hv_arch_context_save` / `hv_arch_context_restore` in `arch/arm64/context.S` (replace weak-symbol stubs)
- Add cooperative yield path: FreeRTOS `vPortYield()` triggers HVC → EL2 records budget tick
- Priority inversion test: confirm `hv_freertos_task_unblock()` ceiling-protocol enforcement on real RTOS workload

**Files affected:**

| File | Change |
| ---- | ------ |
| `arch/arm64/context.S` | Implement full GP-register + FPSIMD save/restore for EL2-mediated context switch |
| `arch/arm64/boot.S` | Add PSCI error decode after `hvc #0`; log `x0` return code via hypervisor UART |
| `src/core/partition.c` | Remove `partition_b_secondary_cpu_init` placeholder; use real platform init sequence |
| `src/guest/rtos/freertos_integration.c` | Replace `HV_ARCH_CONTEXT_STUB` weak symbols with calls to arch/arm64/context.S exports |
| `tests/integration/test_secondary_cpu_isolation.c` | Add T7: PSCI error-path rejection (invalid MPIDR), T8: FreeRTOS task period fidelity |
| `tests/benchmarks/bench_isolation_latency.c` | Add FreeRTOS yield-to-EL2-and-back round-trip latency measurement |

**Exit criteria:**
- [ ] CPU 2 boots to Partition B guest entry via real PSCI on QEMU arm64
- [ ] FreeRTOS task completes 1000 periods with deadline miss rate < 0.1%
- [ ] `hv_arch_context_save` / `hv_arch_context_restore` confirmed by register-comparison selftest
- [ ] Bench report shows median EL2 mediation overhead < 2µs

---

### Phase 6: Static Analysis + Safety Certification Pathway

**Theme:** Gate all merges on cppcheck + clang `--analyze`; establish a MISRA-C subset compliance baseline to support any future ISO 26262 or IEC 61508 claim.

> **Status as of v0.6.0:** The CI workflow (`.github/workflows/static-analysis.yml`) and local script (`scripts/ci/static-analysis.sh`) are complete and passing. The remaining work is the MISRA-C rule selection and deviation documentation.

**Static analysis gate (complete - `feat/static-analysis-gate`):**
- cppcheck: `src/core/`, `src/guest/`, `drivers/` (except `drivers/linux/` - kernel module)
- clang `--analyze`: `src/core/`, `src/guest/` - core, deadcode, security checkers
- Both tools run in CI and locally; `PASS`/`FAIL` exit codes enforce the gate

**MISRA-C subset (future work):**

Select a targeted subset of MISRA-C:2012 rules for the TCB core (`src/core/` only):

| Rule ID | Requirement | Rationale |
| ------- | ----------- | --------- |
| 15.5 | A function have a single point of exit | Auditable control flow |
| 17.7 | Return value of non-void functions not discarded | Prevents silent failure |
| 21.3 | `<stdlib.h>` memory allocation not used | No dynamic allocation in TCB |
| 21.6 | Standard I/O not used in production paths | No `printf` in core |
| 14.4 | Controlling expression of `if` shall be essentially boolean | Reduces implicit conversions |
| 11.1–11.8 | Pointer conversions restricted | Prevents type confusion attacks |

**Deviation record template:**

```markdown
## MISRA-C Deviation DR-001

Rule: 11.1 - Conversions shall not be performed between a pointer to a function and any other type.
File: arch/arm64/boot.S (inline function pointer cast for PSCI entry)
Justification: The PSCI entry point must be cast from uintptr_t to function pointer
  to satisfy the HVC ABI. The cast is reviewed for alignment and type safety.
Review: Isolation Guardian sign-off required before merge.
```

**Files to create:**

| File | Purpose |
| ---- | ------- |
| `docs/safety/MISRA_DEVIATION_RECORD.md` | Deviation log for TCB MISRA-C subset |
| `docs/safety/STATIC_ANALYSIS_POLICY.md` | Tool selection rationale, suppression policy, false-positive registry |
| `scripts/ci/misra-check.sh` | cppcheck MISRA mode runner (when MISRA addon available) |
| `.github/workflows/misra-advisory.yml` | Advisory-only MISRA run (non-blocking, results uploaded as artifact) |

**Exit criteria:**
- [ ] Zero `--enable=warning` findings in `src/core/` with cppcheck
- [ ] Zero security checker findings in `src/core/` with clang `--analyze`
- [ ] MISRA deviation record created for all intentional rule violations
- [ ] Static analysis policy documented and linked from `CONTRIBUTING.md`

---

### Phase 7: Long-Term Maintenance Strategy

**Theme:** Establish processes that keep Haven buildable, secure, and thesis-reproducible beyond the submission window.

#### 7.1 Version Management

Haven uses [Semantic Versioning](https://semver.org/):

```
MAJOR.MINOR.PATCH[-prerelease]
```

| Increment | When |
| --------- | ---- |
| PATCH | Bug fixes, documentation corrections, test additions that don't change API |
| MINOR | New isolation features, new platform support, new drivers - backwards-compatible ABI |
| MAJOR | Breaking API changes, isolation model changes, TCB restructuring |

**Branching model:**

```
main          ← always releasable; protected branch; requires PR + CI pass
feat/*        ← feature branches; squash-merged to main
fix/*         ← bug fix branches
docs/*        ← documentation-only branches
chore/*       ← non-functional changes (CI, deps, formatting)
release/vX.Y  ← release preparation branches; tagged and frozen after release
```

**Backport policy:**
- Critical security fixes: backport to the two most recent MINOR releases
- Bug fixes that affect thesis reproducibility: backport to `release/v1.0` (thesis tag)
- New features: no backport; forward-only

#### 7.2 Security Advisory Process

All security-relevant changes (isolation boundary violations, privilege escalation vectors, SMMU bypass paths) follow this process:

1. **Discovery**: File a GitHub Security Advisory (private) with CVSS score estimate
2. **Triage**: Isolation Guardian reviews within 48 hours
3. **Fix**: Implemented on a private fork; tested on QEMU + i.MX95
4. **Disclosure**: 90-day coordinated disclosure; advisory published on fix merge
5. **Changelog**: `SECURITY.md` updated; `CHANGELOG.md` entry under `### Security`

Scope of security coverage:

| Threat | Scope | Out of Scope |
| ------ | ----- | ------------ |
| Partition escape (stage-2 bypass) | In scope | Side-channel attacks (Spectre/Meltdown) |
| SMMU bypass (DMA to wrong partition) | In scope | Physical attacks |
| Budget exhaustion (temporal starvation) | In scope | Supply-chain attacks on toolchain |
| PSCI privilege escalation | In scope | Host OS (Linux) vulnerabilities |

#### 7.3 Dependency Pinning

Haven's build chain has no runtime dependencies, but the CI toolchain must be pinned for reproducibility:

```yaml
# .github/workflows/ci.yml - toolchain pinning
env:
  QEMU_VERSION: "8.2.0"
  GCC_AARCH64_VERSION: "13.2.0"
  CPPCHECK_VERSION: "2.13"
  CLANG_VERSION: "17.0.6"
```

- Toolchain versions pinned in `build/ci/metadata.txt` after each release
- Docker image `Dockerfile` pins all tool versions; SHA256 digest recorded in `docs/thesis/REPRODUCIBILITY_APPENDIX.md`
- `dependabot.yml` (GitHub Actions) enabled for workflow action version pins

#### 7.4 Deprecation Policy

- Deprecated APIs marked with `/* HAVEN_DEPRECATED(reason) */` comment block
- Deprecated items remain for two MINOR releases before removal
- Deprecation list maintained in `CHANGELOG.md` under `### Deprecated`

**Files to create/update for Phase 7:**

| File | Change |
| ---- | ------ |
| `docs/contributing/RELEASE_PROCESS.md` | Version bump checklist, tag procedure, changelog update steps |
| `docs/contributing/SECURITY_PROCESS.md` | Advisory process, CVSS triage guide, disclosure timeline |
| `docs/contributing/BACKPORT_POLICY.md` | Criteria for backporting, stable branch management |
| `.github/dependabot.yml` | Automated action version updates |
| `docs/thesis/REPRODUCIBILITY_APPENDIX.md` | Full commands, Docker SHA, expected outputs, variance ranges |

**Exit criteria:**
- [ ] `RELEASE_PROCESS.md` reviewed and agreed by thesis supervisor
- [ ] `SECURITY_PROCESS.md` linked from `SECURITY.md`
- [ ] Toolchain versions in `build/ci/metadata.txt` match pinned values in workflow
- [ ] `dependabot.yml` active and first auto-PR handled

---

### Phase 8: Community + Publication Pathway

**Theme:** Prepare Haven for external visibility - conference submission, reproducibility packaging, and optional open-source community seeding.

#### 8.1 Target Venues

| Venue | Type | Deadline window | Why Haven fits |
| ----- | ---- | --------------- | -------------- |
| ECRTS | Conference | January (full) / March (WiP) | Real-time isolation on AMP - core topic |
| RTSS | Conference | May | Mixed-criticality, temporal isolation |
| DATE | Conference | September | Embedded systems, SoC-level isolation |
| EMSOFT | Workshop→conf | April | Embedded software, safety-critical OS |
| OSDI / EuroSys | Conference | October | Systems software, if TCB story is strong |

**Paper structure recommendation** (for ECRTS 10-page limit):
1. Introduction + Motivation (1 page): AMP isolation gap, TCB minimality argument
2. Background (1 page): EL2 architecture, SMMUv3, GICv3 virtualization
3. Design (2 pages): 5-layer isolation model, static partitioning decisions, PSCI secondary CPU
4. Implementation (1.5 pages): key code paths, TCB size, formal proof structure
5. Evaluation (3 pages): latency benchmarks (QEMU + i.MX95), deadline miss rates, fault injection results, comparison table vs Jailhouse/Bao
6. Related Work (0.5 page): Jailhouse, Bao, Omnivisor, seL4
7. Conclusion (0.5 page): summary, open problems, future work

#### 8.2 Reproducibility Package

For any submission, a companion reproducibility package must be submitted (or posted to Zenodo):

```
haven-repro-v1.0.tar.gz
├── Dockerfile               ← exact toolchain
├── README-REPRO.md          ← step-by-step from docker run to results
├── scripts/
│   ├── run-all-benchmarks.sh
│   └── generate-paper-figures.py
├── expected-results/
│   ├── qemu-latency.json    ← reference outputs with variance bounds
│   └── fault-injection.json
└── SHA256SUMS               ← integrity manifest
```

**Requirements for reproducibility claim:**
- [ ] Any reviewer can `docker run haven-repro:v1.0 ./run-all-benchmarks.sh` and get matching results
- [ ] Variance tolerance documented: ±15% for latency on QEMU, ±5% on i.MX95
- [ ] All benchmark data archived on Zenodo with DOI
- [ ] Paper figures auto-generated from benchmark JSON (no manual transcription)

#### 8.3 Conference Demo Plan (QEMU-based)

A live QEMU demo showing:
1. Haven boots, prints partition table
2. Partition A (Linux-class) attempts to write to Partition B memory → stage-2 fault logged
3. Partition B (RTOS) completes 100 tasks without deadline miss during Partition A stress
4. SMMU blocks rogue DMA → fault logged

Demo script: `scripts/demo/conference-demo.sh`
- Self-contained; runs on any Linux/macOS host with QEMU 8.x installed
- Expected runtime: < 60 seconds
- Output: pass/fail summary + latency histogram

#### 8.4 Open Source Community Seeding (Optional)

If the thesis receives a passing grade and the supervisor approves public release:

1. **License audit**: Confirm Apache-2.0 coverage for all files; check for GPL contamination from any driver code
2. **Clean history**: `git filter-repo` to remove any accidental credentials or large binary blobs
3. **Issue templates**: Create GitHub issue templates for bug reports, feature requests, and platform bring-up requests
4. **Discussions**: Enable GitHub Discussions for Q&A; link from `README.md`
5. **Good first issues**: Tag 5–10 issues as `good-first-issue` (e.g., adding a new UART driver, porting to a new board)

**Files to create for Phase 8:**

| File | Purpose |
| ---- | ------- |
| `scripts/demo/conference-demo.sh` | Self-contained QEMU live demo |
| `docs/thesis/PAPER_OUTLINE.md` | Paper structure, argument chain, evidence mapping |
| `docs/contributing/GOOD_FIRST_ISSUES.md` | Curated list of entry-point tasks for new contributors |
| `.github/ISSUE_TEMPLATE/bug_report.md` | Structured bug report template |
| `.github/ISSUE_TEMPLATE/platform_bringup.md` | Platform bring-up request template |
| `.github/ISSUE_TEMPLATE/feature_request.md` | Feature request template |

**Exit criteria:**
- [ ] `conference-demo.sh` runs end-to-end on fresh macOS host with QEMU 8.x
- [ ] `PAPER_OUTLINE.md` reviewed and covers all 8 ECRTS paper sections
- [ ] Reproducibility package tested by at least one independent reviewer
- [ ] Zenodo DOI assigned for benchmark dataset

---

## Part 9: Implementation Progress Tracker

This section tracks completed milestones against the 12-month plan. Updated at each release tag.

### Completed Milestones

| PR | Branch | Description | Release |
| -- | ------ | ----------- | ------- |
| #1–9 | various | CI/CD foundation, core contracts, test infrastructure | v0.1.0-dev |
| #10–15 | various | SMMU 8-scenario coverage, EL2 exception injection, benchmark suite | v0.4.0 |
| #16–22 | various | QEMU two-partition smoke test, IRQ injection, temporal isolation demo | v0.5.0 |
| #23 | `feat/secondary-cpu-bringup` | Secondary CPU (CPU 2) PSCI bring-up, Partition B AMP launch | v0.6.0 |
| #24 | `feat/freertos-context-hardening` | FreeRTOS context-switch state machine, task block/unblock, priority ceiling | v0.6.0 |
| #25 | `chore/website-v0.6.0-sync` | Changelog, architecture overview, thesis traceability updated for v0.6.0 | v0.6.0 |
| #26 | `feat/static-analysis-gate` | cppcheck + clang `--analyze` CI gate; local `scripts/ci/static-analysis.sh` | v0.6.0 |
| #31 | `chore/cmake-migration` | Full CMake ≥ 3.22 + Ninja migration; Makefile retired; CMakePresets.json with 4 presets | v0.6.1 |
| #32 | `docs/cmake-migration` | Docs and website updated to cmake preset commands; Getting-Started updated | v0.6.1 |
| #33–34 | `fix/benchmark-baseline-and-make-refs` | benchmark-baseline.py corrected paths; remaining make→cmake cleanup in CI workflows, scripts, docs | v0.6.1 |

### Open Milestones

| ID | Description | Phase | Priority |
| -- | ----------- | ----- | -------- |
| M-P5-1 | Implement `hv_arch_context_save/restore` in `arch/arm64/context.S` | Phase 5 | High |
| M-P5-2 | FreeRTOS yield → HVC → EL2 budget tick path | Phase 5 | High |
| M-P5-3 | PSCI error-code decode and logging | Phase 5 | Medium |
| M-P6-1 | MISRA-C deviation record for `src/core/` | Phase 6 | Medium |
| M-P6-2 | `docs/safety/STATIC_ANALYSIS_POLICY.md` | Phase 6 | Low |
| M-P7-1 | `docs/contributing/RELEASE_PROCESS.md` | Phase 7 | Medium |
| M-P7-2 | `docs/thesis/REPRODUCIBILITY_APPENDIX.md` | Phase 7 | High |
| M-P8-1 | `scripts/demo/conference-demo.sh` | Phase 8 | Medium |
| M-P8-2 | `docs/thesis/PAPER_OUTLINE.md` | Phase 8 | High |
| M-P4-1 | Coq formal proofs (`verification/coq/`) | Phase 4 | High |
| M-P4-2 | i.MX95 measured benchmark campaign | Phase 4 | High |

### Quality Gate Dashboard

| Gate | Status | Notes |
| ---- | ------ | ----- |
| ARM64 binary builds (cross-compile) | ✅ | `cmake --preset arm64-qemu && cmake --build build` produces `build/haven.elf` |
| All unit + integration tests pass | ✅ | 46 FreeRTOS + secondary CPU tests pass |
| QEMU boots to isolation demo | ✅ | `scripts/qemu/qemu-smoke.sh` - Partition A + B markers pass |
| Static analysis gate | ✅ | cppcheck + clang `--analyze` both PASS (as of #26) |
| Benchmark baseline captured | ✅ | 15/15 benchmarks in `build/benchmarks/`; actual data in `docs/methodology/BENCHMARK_BASELINE.md` |
| Traceability matrix Ch1–5 | ✅ | Ch1–5 rows complete with evidence paths; Ch6–8 present |
| i.MX95 evidence package | ⬜ | Phase 3 / R3 - in progress |
| Formal proofs check | ⬜ | Phase 4 / R4 - not started |
| RTOS latency ≤ threshold | 🔄 | QEMU baseline captured (all PASS); i.MX95 hardware run pending |
| Publication-quality paper draft | ⬜ | Phase 8 - not started |
