# Haven Architecture Overview

## Research Question

Can a minimal EL2 hypervisor - with a TCB under 5,000 lines of C - provide
provably correct spatial and temporal isolation between mixed-criticality
partitions (a Linux general-purpose OS and a hard-RTOS) on ARM64 hardware,
with latency overhead below 100 µs?

Haven's thesis argues *yes*, through a combination of formal policy proofs
(Coq, Isabelle/HOL) and empirical validation on QEMU and i.MX95 hardware.

**Current version:** v0.6.0 - EL2 virtual IRQ injection, Partition B RTOS
stub, SMMU 8-scenario integration test, CI benchmark regression gate.

---

## System Architecture

```
                    ┌─────────────────────────────────────┐
                    │           EL2 Hypervisor             │
                    │                                      │
                    │  ┌───────────────┐ ┌─────────────┐  │
                    │  │  Partition A  │ │ Partition B  │  │
                    │  │ (Linux / EL1) │ │ (RTOS / EL1) │  │
                    │  └───────────────┘ └─────────────┘  │
                    │                                      │
                    │  ┌──────────────────────────────┐   │
                    │  │   Isolation Policy Layer      │   │
                    │  │  stage2 │ irq │ smmu │ timer  │   │
                    │  └──────────────────────────────┘   │
                    │                                      │
                    │  ┌──────────────────────────────┐   │
                    │  │  ARM64 Hardware Binding       │   │
                    │  │  arch/arm64/ │ drivers/       │   │
                    │  └──────────────────────────────┘   │
                    └─────────────────────────────────────┘
                               ARM64 Hardware
                      GICv3 │ SMMUv3 │ Generic Timer
```

Haven is a **static-partition hypervisor**: all partition boundaries
(CPU affinity, physical memory regions, interrupt ownership, device
assignment) are fixed at boot time from a YAML configuration file.
There is no dynamic partition creation or VM migration.  This constraint
enables a dramatically smaller trusted computing base than general-purpose
hypervisors (Xen, KVM, QEMU) while making formal verification tractable.

---

## Design Principles

1. **Isolation-first** - every design decision prioritises correctness of
   isolation over feature richness.  Features that cannot be isolated
   deterministically are not implemented.
2. **Minimal TCB** - the hypervisor core (`src/core/`) is the only trusted
   component.  Guest drivers, the guest OS kernel, and RTOS code all run at
   EL1/EL0 and cannot compromise isolation invariants even if buggy.
3. **Static configuration** - partition layout is read from YAML at boot and
   never mutated at runtime.  This eliminates a large class of TOCTOU and
   reconfiguration vulnerabilities.
4. **Formal contracts at the policy layer** - isolation invariants are
   expressed as Coq/Isabelle theorems over a portable abstract model, then
   validated against the C implementation through testing.

---

## Five-Layer Isolation Model

Haven enforces isolation through five orthogonal mechanisms:

### Layer 1 - Stage-2 MMU (CPU Memory Isolation)

ARM64 stage-2 translation maps each partition's IPA (Intermediate Physical
Address) space to a disjoint region of physical RAM.  The hypervisor configures
`VTTBR_EL2` per partition and enforces that no two partitions' stage-2 maps
overlap.  Implemented in `src/core/mm/stage2.c`; formalised in
`verification/coq/IsolationModel.v` (`spatial_isolation` theorem).

### Layer 2 - SMMU (DMA Isolation)

The ARM SMMUv3 provides a second-stage translation for DMA-capable peripherals.
Every DMA transfer from a device assigned to Partition A is constrained to
Partition A's physical memory window.  Implemented in
`src/core/dma/smmu.c` and `drivers/iommu/smmu_v3.c`.

Enforced policies (v0.6.0, 8 integration scenarios):
- Stream ID ownership exclusivity (`HV_EPERM` on cross-partition configure).
- DMA window read/write permission enforcement (S6/S7: RO/WO window violations).
- StreamID pool exhaustion hard cap (`HV_ENOSPC`, S8).
- `reset_partition` releases both configured and unconfigured StreamID
  allocations (pool-leak fix, PR #19).

### Layer 3 - IOMMU Group Ownership (Device Assignment)

Peripherals are statically assigned to partitions in the YAML config.  Haven
enforces an *exclusive ownership* model: a device can belong to at most one
partition.  Cross-partition device sharing is rejected at boot.  Implemented in
`src/core/iommu/iommu_policy.c`.

### Layer 4 - IRQ Ownership (Interrupt Routing)

GICv3 interrupt routing tables are programmed at boot to deliver each IRQ
exclusively to its owning partition's virtual CPU interface.  Cross-partition
interrupt injection (e.g., a malicious guest triggering a SGI to another
partition) is blocked by the `irq_ownership` check in
`src/core/irq/ownership.c`.

**Virtual IRQ injection** (v0.6.0, PR #17): `hv_el2_inject_exception()` writes
a free GICv3 List Register (`ICH_LR<n>_EL2`) with Pending state to deliver a
virtual IRQ to the target partition.  VBAR_EL2 installation (`hv_install_vectors`)
and GICv3 virtual CPU interface setup (`hv_arch_gic_el2_setup`) are wired in
`hv_el2_exceptions_init()`.  35/35 EL2 unit tests pass.

### Layer 5 - Budget Scheduler + Timer (Temporal Isolation)

On shared cores, partitions are allocated CPU time budgets (in timer ticks)
per scheduling period.  When a partition's budget is exhausted, Haven preempts
it and suspends it until the next period.  The budget invariant
(`budget <= period`) is formalised in `verification/coq/BudgetScheduler.v`.
Implemented in `src/core/sched/budget.c` and `src/core/time/timer.c`.

---

## Hardware Binding Layer

The hardware binding layer translates the policy layer's abstract operations
into concrete ARM64 register and memory-mapped I/O writes.

```
arch/arm64/
  entry.S          - EL2 entry, exception vector table
  mm.c             - Stage-2 page table walk and manipulation
  gic.c            - GICv3 initialisation (delegates to drivers/irqchip/)
  smmu.c           - SMMUv3 initialisation (delegates to drivers/iommu/)
  timer.c          - Generic Timer EL2 programming

drivers/
  irqchip/         - GICv3 driver (gic_v3.c)
  iommu/           - SMMUv3 driver (smmu_v3.c)
  uart/            - Debug UART (PL011, i.MX UART)
  linux/           - Linux guest kernel module (/dev/haven)
  guest-tools/     - Guest CLI tool (haven_tool)
```

The hardware binding layer is *outside* the formal TCB: it is validated by
the test suite (`tests/`) rather than by proof.

---

## Comparison with Related Work

| Feature                         | Haven              | Jailhouse | Bao      | seL4           |
| ------------------------------- | ------------------ | --------- | -------- | -------------- |
| Static partition model          | Yes                | Yes       | Yes      | Yes            |
| AMP support (mixed-criticality) | Yes                | Yes       | Yes      | Partial        |
| Hard-RTOS partition support     | Yes                | Yes       | Yes      | Yes            |
| Formal isolation proof          | Yes (Coq+Isabelle) | No        | No       | Yes (Isabelle) |
| TCB size (approx.)              | < 5 KLOC           | ~12 KLOC  | ~8 KLOC  | ~9 KLOC        |
| Dynamic partition creation      | No                 | No        | No       | Yes            |
| Stage-2 + SMMU isolation        | Yes                | Yes       | Yes      | Partial        |
| Budget scheduler                | Yes                | No        | Yes      | Via MCS        |
| Open thesis artefact            | Yes                | Open src  | Open src | Open src       |

Haven's distinguishing claim is the combination of a *sub-5 KLOC TCB*,
*formal policy proofs*, and empirical validation on *real mixed-criticality
hardware* (i.MX95 with both Linux and FreeRTOS partitions).

---

## TCB Size

```bash
# Count the trusted core - excludes drivers/, arch/, tests/, tools/
wc -l src/core/*.c src/core/**/*.c include/haven/*.h
# Target: < 5,000 lines
```

The `arch/` and `drivers/` layers are excluded from the TCB count because
they are validated by testing, not formal proof, and could in principle be
replaced with verified equivalents in future work.

---

## Source Layout

```
src/core/
  mm/        - Stage-2 MMU management
  dma/       - SMMU/DMA protection
  irq/       - IRQ ownership tables
  iommu/     - IOMMU group policy
  sched/     - Budget scheduler
  time/      - Generic Timer management
  exc/       - EL2 exception handling
  init.c     - Hypervisor boot sequence

include/haven/
  stage2.h   - Stage-2 map/unmap API
  types.h    - Shared type definitions
  iommu.h    - IOMMU policy API
  timer.h    - Timer API
  platform.h - Platform abstraction

verification/
  coq/       - Coq 8.18 policy proofs
  isabelle/  - Isabelle/HOL 2023 cross-validation

tests/
  unit/      - Per-module unit tests
  integration/ - Full partition bring-up tests
  isolation/   - Spatial and temporal isolation tests
  benchmarks/  - Latency and throughput measurements
               latency-baseline.json - 15-entry regression baseline (committed)
  demos/     - Guest EL1 stubs (Partition A + B)
```

---

## Further Reading

- `docs/architecture/ISOLATION_MODEL.md` - detailed isolation mechanism descriptions
- `docs/safety/THREAT_MODEL.md` - threat model and assumptions
- `verification/coq/README.md` - Coq proof guide
- `docs/methodology/BENCHMARK_BASELINE.md` - benchmark baselines and acceptance thresholds
- `docs/methodology/CHAPTER_TRACEABILITY.md` - chapter↔artifact traceability matrix (v0.6.0 milestone summary)
