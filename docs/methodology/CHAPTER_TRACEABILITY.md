# Chapter to Milestone Traceability

This matrix maps every thesis chapter to implementation artifacts, test files,
evidence artifacts, and measured results.  All file paths are relative to the
repository root.

---

## Chapter 1 — Problem and Motivation

**Goal:** Define the mixed-criticality isolation problem on heterogeneous SoCs
and justify the need for a thin, verifiable EL2 hypervisor layer.

| Artifact type       | Path                                              |
|---------------------|---------------------------------------------------|
| Architecture doc    | `docs/architecture/OVERVIEW.md`                  |
| Threat model        | `docs/safety/THREAT_MODEL.md`                    |
| Safety assumptions  | `docs/safety/ASSUMPTIONS.md`                     |

**Acceptance criteria:** Scope review acceptance; baseline architecture doc
reviewed and signed off by thesis supervisor.

---

## Chapter 2 — Related Work

**Goal:** Position Haven against Jailhouse, Bao, seL4, Hafnium, and
type-1 automotive hypervisors; identify isolation gaps in the prior art.

| Artifact type       | Path                                              |
|---------------------|---------------------------------------------------|
| Isolation model     | `docs/architecture/ISOLATION_MODEL.md`           |
| Comparative notes   | `docs/roadmap/DESCRIPTION.md`                    |

**Acceptance criteria:** Comparative requirements checklist complete;
differentiating claims referenced to prior-art sources.

---

## Chapter 3 — System Design

**Goal:** Specify the static partition model, EL2 enforcement boundaries, and
interface contracts between subsystems.

| Artifact type          | Path                                                            |
|------------------------|-----------------------------------------------------------------|
| Public API headers     | `include/haven/types.h`                                        |
|                        | `include/haven/stage2.h`                                       |
|                        | `include/haven/smmu.h`                                         |
|                        | `include/haven/iommu.h`                                        |
|                        | `include/haven/timer.h`                                        |
|                        | `include/haven/platform.h`                                     |
|                        | `include/haven/string.h`                                       |
| Architecture layer API | `arch/arm64/include/asm/sysregs.h`                             |
| Repository structure   | `docs/architecture/REPOSITORY_STRUCTURE.md`                    |

**Acceptance criteria:** Interface review complete; all public API headers
compile cleanly under `make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu- all`.

---

## Chapter 4 — Spatial Isolation

**Goal:** Enforce memory and DMA separation between partitions at runtime;
demonstrate that unauthorized access paths are closed.

### 4.1 Implementation artifacts

| Subsystem           | Source file(s)                                    |
|---------------------|---------------------------------------------------|
| Stage-2 MMU         | `src/core/mm/stage2.c`                           |
| SMMU/DMA policy     | `src/core/dma/smmu.c`                            |
| IOMMU group policy  | `src/core/iommu/iommu_policy.c`                  |
| GICv3 IRQ routing   | `src/core/irq/ownership.c`                       |
| GICv3 driver        | `drivers/irqchip/gic_v3.c`                       |
|                     | `drivers/irqchip/gic_v3.h`                       |
| SMMUv3 driver       | `drivers/iommu/smmu_v3.c`                        |
| ARM64 MMU arch      | `arch/arm64/mm.c`                                |
| ARM64 IRQ arch      | `arch/arm64/irq.c`                               |
| ARM64 CPU init      | `arch/arm64/cpu.c`                               |
| System registers    | `arch/arm64/include/asm/sysregs.h`               |

### 4.2 Test files

| Scenario                              | File                                                       |
|---------------------------------------|------------------------------------------------------------|
| Spatial boundary positive/negative   | `tests/isolation/test_spatial_isolation.c`                |
| Full happy-path integration           | `tests/integration/test_isolation_flow.c`                 |
| Negative isolation paths              | `tests/integration/test_isolation_negative.c`             |
| SMMU hardware integration             | `tests/integration/test_smmu_hardware.c`                  |
| Fault injection matrix F1–F8          | `tests/integration/test_fault_injection.c`                |
| Hypervisor invariant self-tests       | `tests/selftests/test_hypervisor_invariants.c`            |
| SMMU unit test                        | `tests/unit/test_smmu_dma.c`                              |
| IOMMU policy unit test                | `tests/unit/test_iommu_policy.c`                          |
| Two-partition demo                    | `tests/demos/demo_two_partition.c`                        |

### 4.3 Fault injection coverage

| ID  | Description                              | Expected result           |
|-----|------------------------------------------|---------------------------|
| F1  | Stage-2 cross-partition memory escape    | HV_EPERM, state intact    |
| F2  | IRQ ownership pre-emption                | HV_EPERM, route unchanged |
| F3  | Budget overrun after period boundary     | HV_EPERM, budget capped   |
| F4  | SMMU cross-partition DMA escape          | HV_EPERM, window blocked  |
| F5  | Timer deadline hijack                    | HV_EPERM, victim unharmed |
| F6  | IOMMU group ownership theft              | HV_EPERM, group retained  |
| F7  | EL2 IRQ route hijack                     | HV_EPERM, route exclusive |
| F8  | Compound multi-module fault sequence     | All denials, state intact |

### 4.4 Evidence artifacts

- Unit and integration test run logs: `build/tests/`
- Config lint output: produced by `scripts/check-configs.sh`
- Evidence package: `scripts/package-evidence.sh`

### 4.5 Measured results (QEMU baseline)

- Stage-2 map/unmap cycle: < 5 µs median on QEMU aarch64
- SMMU check_dma_access hot path: O(1) table lookup
- Fault injection matrix: 8/8 scenarios PASS on every CI run
- Spatial isolation test suite: 100 % pass rate; see `build/benchmarks/baseline.json`

---

## Chapter 5 — Temporal Isolation

**Goal:** Enforce CPU budget and interrupt-ownership discipline; demonstrate
that RTOS partition response time remains bounded under Linux-side stress.

### 5.1 Implementation artifacts

| Subsystem            | Source file(s)                                   |
|----------------------|--------------------------------------------------|
| Budget scheduler     | `src/core/sched/budget.c`                        |
| Timer management     | `src/core/time/timer.c`                          |
| IRQ ownership        | `src/core/irq/ownership.c`                       |
| ARM64 timer arch     | `arch/arm64/timer.c`                             |
| ARM64 IRQ arch       | `arch/arm64/irq.c`                               |
| EL2 exception handler| `src/core/exc/el2_exceptions.c`                  |

### 5.2 Test files

| Scenario                              | File                                                        |
|---------------------------------------|-------------------------------------------------------------|
| Temporal boundary positive/negative  | `tests/isolation/test_temporal_isolation.c`                |
| Budget unit tests                     | `tests/unit/test_core_stubs.c`                             |
| Timer unit tests                      | `tests/unit/test_timer.c`                                  |
| Temporal isolation benchmark          | `tests/benchmarks/bench_temporal_isolation.c`              |
| Isolation latency benchmark           | `tests/benchmarks/bench_isolation_latency.c`               |
| Stage-2 fault containment benchmark   | `tests/benchmarks/bench_stage2_fault.c`                    |
| SMMU DMA policy benchmark             | `tests/benchmarks/bench_smmu_policy.c`                     |

### 5.3 Acceptance criteria

- Budget overrun is detected and denied within one scheduler tick (HV_EPERM returned).
- `hv_budget_consume` rejects excess beyond configured `budget_ns` bound.
- `hv_timer_set_deadline` is blocked while an unacknowledged expiry is active
  (fail-closed expired-before-acknowledged guard).
- RTOS task latency/jitter: evaluated threshold documented in
  `docs/methodology/EVALUATION_PLAN.md`.

### 5.4 Measured results (QEMU baseline)

- Budget accounting: deterministic O(1) per tick; no heap allocation.
- Temporal isolation benchmark: deadline miss count = 0 across 1000 iterations
  in QEMU smoke scenario.
- See `build/benchmarks/baseline.json` for trend data.

---

## Chapter 6 — Formal Verification (Planned)

**Goal:** Provide machine-checked proofs of the key isolation invariants for
the stage-2 mapping and budget scheduler modules.

### 6.1 Planned artifact locations

| Tool        | Directory                          |
|-------------|------------------------------------|
| Coq         | `verification/coq/*.v`            |
| Isabelle    | `verification/isabelle/*.thy`      |

### 6.2 Invariants targeted

1. Stage-2 map is partition-scoped: `hv_stage2_map_partition` never overlaps
   existing PA regions owned by other partitions.
2. Budget accounting is monotone: `budget_ns` decreases only by
   `hv_budget_consume` and never below zero without a period reset.
3. IRQ ownership is exclusive: no two partitions hold the same IRQ route.

### 6.3 Status

Formal verification is scheduled for Phase 3 (R3/M4).  Until proofs are
committed, the invariants are covered by the negative integration tests and
the hypervisor invariant self-tests (`tests/selftests/test_hypervisor_invariants.c`).

---

## Chapter 7 — Evaluation

**Goal:** Quantify isolation behavior under deliberate interference; provide
WCET, jitter, and deadline-miss metrics for thesis claims.

### 7.1 Implementation artifacts (analysis tooling)

| Tool / script                   | Path                                       |
|---------------------------------|--------------------------------------------|
| Benchmark baseline collector    | `scripts/benchmark-baseline.py` (planned) |
| Evidence packager               | `scripts/package-evidence.sh`             |
| CI preflight                    | `scripts/ci-preflight.sh`                  |
| QEMU smoke runner               | `scripts/qemu-smoke.sh`                    |
| Traceability checker            | `scripts/check-traceability.sh`            |
| CI validation                   | `scripts/ci-validate.sh`                   |
| Evidence comparison             | `scripts/compare-evidence.py`              |

### 7.2 Test files providing evaluation evidence

| File                                                  | Evidence class         |
|-------------------------------------------------------|------------------------|
| `tests/benchmarks/bench_isolation_latency.c`         | Latency / jitter       |
| `tests/benchmarks/bench_temporal_isolation.c`        | Deadline miss count    |
| `tests/benchmarks/bench_stage2_fault.c`              | Stage-2 fault latency  |
| `tests/benchmarks/bench_smmu_policy.c`               | DMA policy throughput  |
| `tests/integration/test_fault_injection.c`           | Fault containment      |
| `tests/integration/test_smmu_hardware.c`             | Hardware DMA coherence |

### 7.3 Benchmark output locations

- Local runs: `build/benchmarks/baseline.json`
- CI artifacts: `benchmark-<compiler>-<run-id>/baseline.json`

### 7.4 Acceptance criteria

- Fault injection matrix: all 8 scenarios PASS.
- Latency benchmark: median stage-2 fault containment < 10 µs on QEMU.
- Deadline miss benchmark: 0 misses in 1 000-iteration run.
- Board-backed evidence: IMX95 validation runbook completed (R3/M5 gate).
  Template: `docs/methodology/IMX95_EVIDENCE_TEMPLATE.md`

---

## Chapter 8 — Conclusions and Future Work

**Goal:** Document the limits of the current implementation, identify paths
toward safety certification (ISO 26262, IEC 61508), and specify future
verification depth.

| Artifact type       | Path                                              |
|---------------------|---------------------------------------------------|
| Safety assumptions  | `docs/safety/ASSUMPTIONS.md`                     |
| Threat model        | `docs/safety/THREAT_MODEL.md`                    |
| Roadmap             | `docs/roadmap/DESCRIPTION.md`                    |
| Evaluation plan     | `docs/methodology/EVALUATION_PLAN.md`            |
| AI workflow notes   | `docs/methodology/AI_WORKFLOW_PLAYBOOK.md`       |

**Acceptance criteria:** Gap analysis complete; roadmap milestones linked to
chapter claims; future-work section references open issues.

---

## Acceptance Criteria by Isolation Dimension

### Spatial isolation

- Guest cannot map or access non-owned memory regions (stage-2 enforced).
- DMA from a non-owner partition is blocked by SMMU policy (`HV_EPERM`).
- Interrupt route leakage across forbidden domains is denied (`HV_EPERM`).
- Evidence: `tests/isolation/test_spatial_isolation.c`,
  `tests/integration/test_smmu_hardware.c`,
  fault injection F1 and F4.

### Temporal isolation

- Budget accounting prevents sustained overrun beyond configured `budget_ns`.
- RTOS partition response time remains within evaluated threshold under Linux stress.
- Scheduler and timer bookkeeping remain monotonic and deterministic.
- Timer deadline fail-closed guard blocks new deadline while expiry is pending.
- Evidence: `tests/isolation/test_temporal_isolation.c`,
  `tests/benchmarks/bench_temporal_isolation.c`,
  fault injection F3 and F5.

### IRQ isolation

- IRQ ownership is exclusive: `hv_irq_assign` rejects duplicates (`HV_EPERM`).
- EL2 IRQ routing table is scoped per-partition; cross-partition route reuse denied.
- Virtual IRQ injection (`arch/arm64/irq.c`) is gated on prior ownership validation.
- Evidence: `tests/unit/test_core_stubs.c`,
  `tests/integration/test_isolation_negative.c`,
  fault injection F2 and F7.

---

## CI traceability checks

The script `scripts/check-traceability.sh` verifies that every artifact path
listed in this matrix exists in the repository and that all referenced test
binaries are built and pass under `make test`.

Schedule: run on every pull request targeting `main`.
