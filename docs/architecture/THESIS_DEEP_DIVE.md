# Thesis Deep Dive

Title:
Static Partition Hypervisor for Asymmetric Multiprocessing: Enforcing Spatial and Temporal Isolation Between Linux and RTOS on a Heterogeneous SoC

## 1. Why this thesis matters

Modern heterogeneous SoCs combine cores with different execution models in one chip:
- Cortex-A class cores for rich operating systems such as Linux.
- Cortex-M or Cortex-R class cores for deterministic RTOS workloads.

Without hard isolation, faults or compromise in one domain can corrupt memory,
steal device access, or introduce unbounded latency in safety-critical control paths.

This thesis targets a practical question:
- Can a small, static EL2 hypervisor provide strong, measurable isolation on a real AMP SoC while remaining auditable enough for safety-oriented engineering?

## 2. Static partition hypervisor rationale
Haven uses static partitioning to lock resource ownership before runtime:
- Partition boundaries are fixed at configuration/build time.
- CPU, memory, interrupt, and DMA ownership are explicit and immutable during mission mode.

Compared with dynamic virtualization models, static partitioning improves:
- Determinism under load.
- Analysis tractability.
- Certification suitability for standards such as IEC 61508 and ISO 26262.

## 3. Isolation properties and thesis claims

### Spatial isolation

Goal:
Partition A cannot read, write, or DMA into partition B resources.

Mechanisms:
- Stage-2 page-table controls for CPU memory accesses.
- SMMU/IOMMU policy for DMA-capable peripherals.
- Interrupt ownership model with deny-by-default routing.

Expected proof style:
- Positive-path tests show owned resources are usable.
- Negative-path tests show non-owned resources are denied.

### Temporal isolation

Goal:
Overload in one partition does not violate bounded response in another.

Mechanisms:
- Budget-based scheduling with fixed period and budget windows.
- Core pinning for high-criticality domains when feasible.
- Controlled interrupt delivery and accounting at the hypervisor layer.

Expected proof style:
- Budget accounting correctness at unit level.
- End-to-end latency/jitter measurements under Linux interference on QEMU and i.MX95.

## 4. Current implementation maturity snapshot

| Area | Current state | Evidence in repository | Main gap to thesis-ready claim |
| --- | --- | --- | --- |
| Stage-2 partition mapping | Contract-level implementation with overlap and range checks | `src/core/mm/stage2.c`, `tests/unit/test_core_stubs.c`, `tests/integration/test_isolation_negative.c` | Move from software contract model to EL2 page-table programming on target hardware |
| IRQ ownership | Deny-by-default ownership map with conflict rejection | `src/core/irq/ownership.c`, `tests/unit/test_core_stubs.c` | Bind ownership model to real GIC routing and boot-time policy loading |
| Budget scheduler | Per-partition budget/period accounting with overrun denial | `src/core/sched/budget.c`, `tests/unit/test_core_stubs.c` | Add timer-driven period management and measured latency bounds under stress |
| DMA isolation (SMMU) | StreamID and window policy model with overlap checks | `src/core/dma/smmu.c`, `tests/unit/test_smmu_dma.c` | Integrate with platform SMMU programming path and prove behavior on i.MX95 |
| Validation process | CI, config checks, unit + integration tests operational | `.github/workflows/ci.yml`, `scripts/test.sh` | Expand board-backed campaigns and archive reproducible measurement artifacts |
| Thesis evidence process | Traceability, roadmap, and templates exist | `docs/methodology/CHAPTER_TRACEABILITY.md`, `docs/methodology/IMX95_EVIDENCE_TEMPLATE.md` | Fill matrices with run-linked evidence and residual-risk closure |

## 5. Deep analysis of unfinished work (priority-ordered)

1. **Hardware binding gap (highest risk)**
   - Isolation logic is validated as C-level contract code, but thesis claims require demonstrated enforcement at EL2 with real platform control paths.
   - Priority action: map each isolation mechanism to concrete i.MX95 hardware programming sequence and verification log.

2. **Temporal-evidence gap**
   - Budget semantics exist, but measured worst-case behavior under interference is not yet linked to strict acceptance thresholds.
   - Priority action: lock benchmark protocol (workloads, sampling, acceptance bands, rerun policy).

3. **Cross-platform reproducibility gap**
   - QEMU path is CI-friendly; physical board reproducibility needs stronger artifact discipline.
   - Priority action: ensure every board run produces structured evidence package usable by an independent reviewer.

4. **Traceability closure gap**
   - Traceability structure exists, but final thesis chapters need objective artifact links per claim.
   - Priority action: enforce chapter-to-artifact updates at each milestone close.

## 6. Strategy to continue next missions

### Mission A (near-term): convert stubs into platform-enforced paths
- Target milestones: M2 + M3.
- Deliverables:
  1. Stage-2 path updates tied to partition configs and negative tests.
  2. IRQ ownership flow tied to real interrupt controller routing constraints.
  3. SMMU policy binding for DMA-capable devices with fail-closed defaults.
- Exit gate:
  - Unauthorized memory/IRQ/DMA attempts are rejected in both virtual and board-backed runs.

### Mission B (mid-term): temporal isolation and deterministic scheduling evidence
- Target milestone: M4.
- Deliverables:
  1. Timer-driven budget period handling with bounded accounting behavior.
  2. Stress scenarios (Linux interference) with RTOS latency metrics (WCET, jitter, deadline misses).
  3. Regression thresholds integrated into benchmark workflow.
- Exit gate:
  - Latency bounds remain within agreed thresholds across repeated runs.

### Mission C (mid-term): platform validation hardening
- Target milestone: M5.
- Deliverables:
  1. Full i.MX95 validation campaign via runbook.
  2. Cross-OS QEMU reproducibility checks maintained in CI.
  3. Secondary platform sanity validation for assumption cross-check.
- Exit gate:
  - Evidence package contains reproducible logs, metadata, and comparison artifacts.

### Mission D (final): thesis lock and submission packaging
- Target release: R4.
- Deliverables:
  1. Full regression rerun and freeze.
  2. Final chapter traceability completion with artifact IDs/paths.
  3. Final limitations, assumptions, and residual-risk closure report.
- Exit gate:
  - Independent reviewer can reproduce key results from repository runbooks.

## 7. Operating model for weekly execution

1. **Claim-first planning**
   - Start each work item from thesis claim impact (spatial, temporal, reproducibility, or safety narrative).
2. **Fail-closed implementation**
   - Keep deny-by-default behavior for mapping, interrupts, and DMA.
3. **Evidence-coupled definition of done**
   - Code change is not complete until tests, runbook steps, and traceability row updates exist.
4. **Milestone review ritual**
   - At milestone close: update `CHAPTER_TRACEABILITY.md`, `EVALUATION_PLAN.md`, and release notes together.

## 8. Measurable success criteria before thesis submission

1. Spatial isolation:
   - Repeated negative tests show no unauthorized memory/IRQ/DMA access on QEMU and i.MX95.
2. Temporal isolation:
   - RTOS latency/jitter/deadline behavior remains within predefined bounds under stress.
3. Reproducibility:
   - Another engineer can run validation runbooks and produce matching artifacts.
4. Documentation integrity:
   - Every chapter claim maps to an implementation artifact plus measured evidence.

## 9. Remaining gaps to track continuously

- Hardware-specific edge cases and device-specific DMA policies.
- Measurement noise interpretation and benchmark drift management.
- Explicit non-goals boundaries (especially side-channel scope).
- Formal verification depth beyond current placeholders in `verification/`.

## 10. Evaluation direction

Primary measurements:
- Cross-partition memory and DMA violation resistance.
- Interrupt ownership correctness under adversarial stimuli.
- Worst-case latency and jitter of RTOS tasks while Linux is stressed.
- Budget overrun detection and containment behavior.

Recommended reference projects for comparative study:
- Jailhouse (Siemens)
- Bao Hypervisor (University of Minho)
- seL4 (formal verification reference)
