# Chapter to Milestone Traceability

This matrix maps every thesis chapter to implementation artifacts, test files,
evidence artifacts, and measured results.  All file paths are relative to the
repository root.

---

## Chapter 1 - Problem and Motivation

**Goal:** Define the mixed-criticality isolation problem on heterogeneous SoCs
and justify the need for a thin, verifiable EL2 hypervisor layer.

| Artifact type      | Path                            |
| ------------------ | ------------------------------- |
| Architecture doc   | `docs/architecture/OVERVIEW.md` |
| Threat model       | `docs/safety/THREAT_MODEL.md`   |
| Safety assumptions | `docs/safety/ASSUMPTIONS.md`    |

**Acceptance criteria:** Scope review acceptance; baseline architecture doc
reviewed and signed off by thesis supervisor.

---

## Chapter 2 - Related Work

**Goal:** Position Haven against Jailhouse, Bao, seL4, Hafnium, and
type-1 automotive hypervisors; identify isolation gaps in the prior art.

| Artifact type     | Path                                   |
| ----------------- | -------------------------------------- |
| Isolation model   | `docs/architecture/ISOLATION_MODEL.md` |
| Comparative notes | `docs/roadmap/DESCRIPTION.md`          |

**Acceptance criteria:** Comparative requirements checklist complete;
differentiating claims referenced to prior-art sources.

---

## Chapter 3 - System Design

**Goal:** Specify the static partition model, EL2 enforcement boundaries, and
interface contracts between subsystems.

| Artifact type          | Path                                        |
| ---------------------- | ------------------------------------------- |
| Public API headers     | `include/haven/types.h`                     |
|                        | `include/haven/stage2.h`                    |
|                        | `include/haven/smmu.h`                      |
|                        | `include/haven/iommu.h`                     |
|                        | `include/haven/timer.h`                     |
|                        | `include/haven/platform.h`                  |
|                        | `include/haven/string.h`                    |
| Architecture layer API | `arch/arm64/include/asm/sysregs.h`          |
| Repository structure   | `docs/architecture/REPOSITORY_STRUCTURE.md` |

**Acceptance criteria:** Interface review complete; all public API headers
compile cleanly under `cmake --preset arm64-qemu && cmake --build build`.

---

## Chapter 4 - Spatial Isolation

**Goal:** Enforce memory and DMA separation between partitions at runtime;
demonstrate that unauthorized access paths are closed.

### 4.1 Implementation artifacts

| Subsystem          | Source file(s)                     |
| ------------------ | ---------------------------------- |
| Stage-2 MMU        | `src/core/mm/stage2.c`             |
| SMMU/DMA policy    | `src/core/dma/smmu.c`              |
| IOMMU group policy | `src/core/iommu/iommu_policy.c`    |
| GICv3 IRQ routing  | `src/core/irq/ownership.c`         |
| GICv3 driver       | `drivers/irqchip/gic_v3.c`         |
|                    | `drivers/irqchip/gic_v3.h`         |
| SMMUv3 driver      | `drivers/iommu/smmu_v3.c`          |
| ARM64 MMU arch     | `arch/arm64/mm.c`                  |
| ARM64 IRQ arch     | `arch/arm64/irq.c`                 |
| ARM64 CPU init     | `arch/arm64/cpu.c`                 |
| System registers   | `arch/arm64/include/asm/sysregs.h` |

### 4.2 Test files

| Scenario                           | File                                           |
| ---------------------------------- | ---------------------------------------------- |
| Spatial boundary positive/negative | `tests/isolation/test_spatial_isolation.c`     |
| Full happy-path integration        | `tests/integration/test_isolation_flow.c`      |
| Negative isolation paths           | `tests/integration/test_isolation_negative.c`  |
| SMMU hardware integration          | `tests/integration/test_smmu_hardware.c`       |
| Fault injection matrix F1–F8       | `tests/integration/test_fault_injection.c`     |
| Hypervisor invariant self-tests    | `tests/selftests/test_hypervisor_invariants.c` |
| SMMU unit test                     | `tests/unit/test_smmu_dma.c`                   |
| IOMMU policy unit test             | `tests/unit/test_iommu_policy.c`               |
| Two-partition demo                 | `tests/demos/demo_two_partition.c`             |

### 4.3 Fault injection coverage

| ID  | Description                           | Expected result           |
| --- | ------------------------------------- | ------------------------- |
| F1  | Stage-2 cross-partition memory escape | HV_EPERM, state intact    |
| F2  | IRQ ownership pre-emption             | HV_EPERM, route unchanged |
| F3  | Budget overrun after period boundary  | HV_EPERM, budget capped   |
| F4  | SMMU cross-partition DMA escape       | HV_EPERM, window blocked  |
| F5  | Timer deadline hijack                 | HV_EPERM, victim unharmed |
| F6  | IOMMU group ownership theft           | HV_EPERM, group retained  |
| F7  | EL2 IRQ route hijack                  | HV_EPERM, route exclusive |
| F8  | Compound multi-module fault sequence  | All denials, state intact |

### 4.4 Hardware demo artifacts (R3 milestone)

| Artifact                   | Path                                   | Purpose                                        |
| -------------------------- | -------------------------------------- | ---------------------------------------------- |
| Guest A EL1 stub           | `tests/demos/guest_a_entry.S`          | Bare-metal Partition A; triggers cross-partition fault |
| Guest A linker script      | `linker-guest.ld`                      | Links guest A at IPA 0x40000000                |
| Guest B EL1 stub           | `tests/demos/guest_b_entry.S`          | Bare-metal Partition B RTOS stub; triggers A→B isolation fault |
| Guest B linker script      | `linker-guest-b.ld`                    | Links guest B at IPA 0x60000000 (PART_B_IPA_BASE) |
| QEMU smoke runner          | `scripts/qemu/qemu-smoke.sh`           | Validates boot + fault + resume end-to-end (both partitions) |
| QEMU run script            | `scripts/qemu/qemu-run.sh`             | Interactive QEMU launch with guest_a.bin + guest_b.bin |
| Memory map                 | `src/platform/qemu-virt/memory.h`      | Corrected PA layout (8 MB Haven, distinct IPAs) |
| Partition config           | `src/core/partition.c`                 | Stage-2 maps: 512 MB DRAM + UART MMIO for A; 64 MB for B |
| EL2 fault handler          | `src/core/exc/el2_exceptions.c`        | ESR/HPFAR decode, violation log, ELR skip; virtual IRQ injection |
| Stage-2 page table builder | `arch/arm64/mm.c`                      | S2AP fix: Normal WB RW / Device nGnRE RW       |

### 4.5 Evidence artifacts

- Unit and integration test run logs: `build/tests/`
- Config lint output: produced by `scripts/compile/check-configs.sh`
- Evidence package: `scripts/evidence/package-evidence.sh`
- **QEMU hardware demo (Partition A)**: `build/smoke/uart.log` — captured UART output showing Partition A boot, violation denial, post-fault resume.
- **SMMU integration**: 8/8 scenarios pass; `tests/integration/test_smmu_hardware.c`.
- **Benchmark regression gate**: `tests/benchmarks/latency-baseline.json` — 15 named entries, all within 10% tolerance; verified by `scripts/ci/bench-regression.sh`.

### 4.6 Measured results (QEMU baseline — v0.6.0)

- `bench_isolation_latency` hot paths: `stage2_partition_contains_pa` and
  `smmu_check_dma_access` measure sub-microsecond on QEMU (100 000 iterations).
- Stage-2 containment boundary cases (hot/cold/low/high): all measured via
  `bench_stage2_fault.c`; results written to `build/benchmarks/stage2-fault.json`.
- SMMU policy cases (hot, boundary, denied, StreamID alloc, RO-window, WO-window, pool
  exhaustion): all measured via `bench_smmu_policy.c`; results written to
  `build/benchmarks/smmu-policy.json`.
- Fault injection matrix: 8/8 scenarios PASS on every CI run.
- Spatial isolation test suite: 100 % pass rate.
- **End-to-end QEMU demo** (R3): guest EL1 stub boots under stage-2, triggers cross-partition
  violation at IPA 0x60000000, Haven logs `ESR=0x93c08006` (EC=0x24, FSC=0x06), advances ELR,
  guest prints "Post-fault check OK". Smoke test: `validation_status=pass`.
- **EL2 virtual IRQ injection**: 35/35 unit tests pass; ICH_LR<n>_EL2 write path verified
  (`fix/el2-inject-virtual-irq`, PR #17).

---

## Chapter 5 - Temporal Isolation

**Goal:** Enforce CPU budget and interrupt-ownership discipline; demonstrate
that RTOS partition response time remains bounded under Linux-side stress.

### 5.1 Implementation artifacts

| Subsystem             | Source file(s)                  |
| --------------------- | ------------------------------- |
| Budget scheduler      | `src/core/sched/budget.c`       |
| Timer management      | `src/core/time/timer.c`         |
| IRQ ownership         | `src/core/irq/ownership.c`      |
| ARM64 timer arch      | `arch/arm64/timer.c`            |
| ARM64 IRQ arch        | `arch/arm64/irq.c`              |
| EL2 exception handler | `src/core/exc/el2_exceptions.c` |

### 5.2 Test files

| Scenario                            | File                                          |
| ----------------------------------- | --------------------------------------------- |
| Temporal boundary positive/negative | `tests/isolation/test_temporal_isolation.c`   |
| Budget unit tests                   | `tests/unit/test_core_stubs.c`                |
| Timer unit tests                    | `tests/unit/test_timer.c`                     |
| Temporal isolation benchmark        | `tests/benchmarks/bench_temporal_isolation.c` |
| Isolation latency benchmark         | `tests/benchmarks/bench_isolation_latency.c`  |
| Stage-2 fault containment benchmark | `tests/benchmarks/bench_stage2_fault.c`       |
| SMMU DMA policy benchmark           | `tests/benchmarks/bench_smmu_policy.c`        |

### 5.3 Acceptance criteria

- Budget overrun is detected and denied within one scheduler tick (HV_EPERM returned).
- `hv_budget_consume` rejects excess beyond configured `budget_ns` bound.
- `hv_timer_set_deadline` is blocked while an unacknowledged expiry is active
  (fail-closed expired-before-acknowledged guard).
- RTOS task latency/jitter: evaluated threshold documented in
  `docs/methodology/EVALUATION_PLAN.md`.

### 5.4 Measured results (QEMU baseline)

- Budget accounting: deterministic O(1) per tick; no heap allocation.
- `bench_temporal_isolation.c` covers 15 cells: 3 RTOS periods (1 ms, 5 ms,
  10 ms) × 5 Linux load levels (0 %, 25 %, 50 %, 75 %, 100 %); results written
  to `build/benchmarks/temporal-isolation.json`.
- `bench_isolation_latency.c` measures `budget_consume` latency: sub-µs
  per call at 100 000 iterations.
- Deadline miss benchmark: target 0 misses under QEMU simulation; hardware
  measurement pending i.MX95 bring-up.

---

## Chapter 6 - Formal Verification

**Goal:** Provide machine-checked proofs of the key isolation invariants for
the stage-2 mapping and budget scheduler modules.

### 6.1 Artifact locations

| Tool     | Directory                     | Status   |
| -------- | ----------------------------- | -------- |
| Coq      | `verification/coq/*.v`        | Complete |
| Isabelle | `verification/isabelle/*.thy` | Complete |

### 6.2 Invariants proved

1. **Spatial isolation** (`IsolationModel.v`): `hv_stage2_map_partition` never
   overlaps existing PA regions owned by other partitions.
   ```coq
   Theorem spatial_isolation :
     forall (s : HvState) (p1 p2 : PartitionId) (pa : PhysAddr),
       p1 <> p2 -> partition_contains_pa s p1 pa ->
       ~partition_contains_pa s p2 pa.
   ```
2. **Stage-2 map monotonicity** (`Stage2Policy.v`): `hv_stage2_map` preserves
   the no-overlap invariant.
3. **Budget invariant** (`BudgetScheduler.v`): `budget_ns ≤ period_ns` always
   holds; `hv_budget_consume` is monotonically decreasing.

### 6.3 Verification CI gate

Nightly CI job `coq-proof-check` (`.github/workflows/nightly.yml`) runs:
```sh
coqc verification/coq/IsolationModel.v \
     verification/coq/Stage2Policy.v \
     verification/coq/BudgetScheduler.v
```
Failure blocks nightly evidence generation.

### 6.4 Status

All three Coq proofs committed and verified under Coq 8.18.  Isabelle theory
cross-validates the same invariants.  Nightly CI gate active since 2026-05-18.

---

## Chapter 7 - Evaluation

**Goal:** Quantify isolation behavior under deliberate interference; provide
WCET, jitter, and deadline-miss metrics for thesis claims.

### 7.1 Implementation artifacts (analysis tooling)

| Tool / script                | Path                                           |
| ---------------------------- | ---------------------------------------------- |
| Latency analyzer             | `tools/analysis/latency_analyzer.py`           |
| Jitter plotter               | `tools/analysis/jitter_plot.py`                |
| Evidence report generator    | `tools/analysis/evidence_report.py`            |
| Evidence packager            | `scripts/evidence/package-evidence.sh`         |
| CI preflight                 | `scripts/ci/ci-preflight.sh`                   |
| QEMU smoke runner            | `scripts/qemu/qemu-smoke.sh`                   |
| Benchmark regression gate    | `scripts/ci/bench-regression.sh`               |
| Benchmark baseline updater   | `scripts/ci/bench-update-baseline.sh`          |
| Latency baseline             | `tests/benchmarks/latency-baseline.json`       |
| Traceability checker         | `scripts/dev/check-traceability.sh`            |
| CI validation                | `scripts/ci/ci-validate.sh`                    |
| Evidence comparison          | `scripts/evidence/compare-evidence.py`         |
| Release validation           | `scripts/release.sh`                           |

### 7.2 Test files providing evaluation evidence

| File                                          | Evidence class                   |
| --------------------------------------------- | -------------------------------- |
| `tests/benchmarks/bench_isolation_latency.c`  | Latency / jitter                 |
| `tests/benchmarks/bench_temporal_isolation.c` | Deadline miss count              |
| `tests/benchmarks/bench_stage2_fault.c`       | Stage-2 fault latency            |
| `tests/benchmarks/bench_smmu_policy.c`        | DMA policy throughput            |
| `tests/integration/test_fault_injection.c`    | Fault containment (F1–F8)        |
| `tests/integration/test_smmu_hardware.c`      | Hardware DMA coherence           |
| `tests/demos/guest_a_entry.S`                 | Hardware end-to-end demo (QEMU) — Partition A |
| `tests/demos/guest_b_entry.S`                 | Hardware end-to-end demo (QEMU) — Partition B RTOS stub |

### 7.3 Benchmark output locations

- Local runs: `build/benchmarks/` (gitignored; regenerated per run)
- Regression baseline: `tests/benchmarks/latency-baseline.json` (committed)
- CI nightly artifacts: uploaded as `benchmark-results` (30-day retention, `.github/workflows/nightly.yml`)

### 7.4 Acceptance criteria

- Fault injection matrix: all 8 scenarios PASS.
- SMMU integration test: all 8 scenarios PASS (S1–S8, `test_smmu_hardware.c`).
- Latency benchmark: median stage-2 fault containment < 10 µs on QEMU.
- Deadline miss benchmark: 0 misses in 1 000-iteration run.
- Benchmark regression: no named hot-path exceeds 10 % regression from
  `tests/benchmarks/latency-baseline.json` (enforced by nightly CI job).
- **QEMU two-partition demo** (R3): smoke test `validation_status=pass`; UART log shows greeting, violation denial, and post-fault resume — archived in `build/smoke/`.
- Board-backed evidence: IMX95 validation runbook completed (R3/M5 gate).
  Template: `docs/methodology/IMX95_EVIDENCE_TEMPLATE.md`

---

## Chapter 8 - Conclusions and Future Work

**Goal:** Document the limits of the current implementation, identify paths
toward safety certification (ISO 26262, IEC 61508), and specify future
verification depth.

| Artifact type      | Path                                       |
| ------------------ | ------------------------------------------ |
| Safety assumptions | `docs/safety/ASSUMPTIONS.md`               |
| Threat model       | `docs/safety/THREAT_MODEL.md`              |
| Roadmap            | `docs/roadmap/DESCRIPTION.md`              |
| Evaluation plan    | `docs/methodology/EVALUATION_PLAN.md`      |
| AI workflow notes  | `docs/methodology/AI_WORKFLOW_PLAYBOOK.md` |

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

The script `scripts/dev/check-traceability.sh` verifies that every artifact
path listed in this matrix exists in the repository and that all referenced
test binaries are built and pass under `cmake --preset host-tests && ctest --test-dir build-host`.

The nightly job `benchmark-regression` in `.github/workflows/nightly.yml`
automatically gates on the 15-entry latency baseline and uploads results as
30-day artifacts for longitudinal comparison.

---

## v0.6.0 milestone summary

| Milestone artifact              | Status  | Evidence location                           |
| ------------------------------- | ------- | ------------------------------------------- |
| Partition B RTOS stub           | ✅ Done  | `tests/demos/guest_b_entry.S`               |
| EL2 virtual IRQ injection wired | ✅ Done  | `src/core/exc/el2_exceptions.c`             |
| SMMU 8-scenario integration     | ✅ Done  | `tests/integration/test_smmu_hardware.c`    |
| SMMU StreamID pool-leak fix     | ✅ Done  | `src/core/dma/smmu.c`                       |
| Benchmark regression gate       | ✅ Done  | `scripts/ci/bench-regression.sh`            |
| Nightly regression CI job       | ✅ Done  | `.github/workflows/nightly.yml`             |
| Release script                  | ✅ Done  | `scripts/release.sh`                        |
| Secondary-CPU ERET (Partition B) | ⏳ Next | Phase 3 — secondary CPU bring-up            |

Schedule: run on every pull request targeting `main`.
