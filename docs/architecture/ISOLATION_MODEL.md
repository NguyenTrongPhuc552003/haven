# Haven Isolation Model

Haven enforces isolation through five orthogonal mechanisms arranged in two
categories: *spatial* (who can access which physical addresses and devices) and
*temporal* (who can use the CPU and for how long).  This document describes
each mechanism in depth, states the formal invariants, and cross-references the
C implementation and Coq/Isabelle proofs.

---

## Spatial Isolation

### Stage-2 MMU (CPU Memory)

**Mechanism.**  ARM64 provides a two-stage address translation when the
hypervisor is active.  Stage 1 (owned by the guest OS kernel) translates
virtual addresses (VA) to intermediate physical addresses (IPA).  Stage 2
(owned by Haven, programmed via `VTTBR_EL2`) translates IPA to physical
address (PA).  The guest OS has no access to stage-2 page tables and cannot
map any PA that Haven has not granted it.

**Configuration.**  At boot, Haven reads the YAML partition config and builds
per-partition stage-2 page tables.  Each partition receives a list of
`(IPA, PA, size, attributes)` entries.  Haven verifies that no two partitions'
PA ranges overlap before committing the tables.

**Invariant.**  For all distinct partitions `p1`, `p2` and all physical
addresses `pa`:
```
spatial_isolation_invariant(s) ∧ p1 ≠ p2 →
partition_contains_pa(s, p1, pa) → ¬partition_contains_pa(s, p2, pa)
```

**C implementation:** `src/core/mm/stage2.c` — `hv_stage2_map_partition`,
`hv_stage2_contains`, `hv_range_overlaps`.

**Coq proof:** `verification/coq/IsolationModel.v` — `spatial_isolation`
theorem (proved, not admitted).

**Isabelle proof:** `verification/isabelle/HavenIsolation.thy` —
`spatial_no_overlap` theorem.

---

### SMMU / DMA Isolation

**Mechanism.**  DMA-capable peripherals can issue memory transactions
independently of the CPU, bypassing stage-2 page tables.  The ARM SMMUv3
provides a second-stage translation for device-issued addresses.  Haven
configures one SMMU context (stream table entry) per device, restricting
DMA to the owning partition's PA window.

**Configuration.**  Stream IDs are extracted from the device tree / YAML
config at boot.  Haven programs the SMMUv3 stream table (`src/core/dma/smmu.c`)
so that each stream maps to the same PA range as its owning partition's
stage-2 map.

**Invariant.**  A device assigned to partition `p` can only DMA to addresses
`pa` such that `partition_contains_pa(s, p, pa)` holds.  Transactions outside
this range generate an SMMU fault (fault model: abort + EL2 fault handler,
never silent pass-through).

**C implementation:** `src/core/dma/smmu.c`, `drivers/iommu/smmu_v3.c`.

**Test coverage:** `tests/unit/test_smmu_dma.c`, `tests/integration/test_isolation_flow.c`.

---

### IOMMU Group Ownership (Device Assignment)

**Mechanism.**  IOMMU groups are the unit of device assignment.  All functions
within a PCIe IOMMU group must be assigned to the same partition (to prevent
intra-group DMA attacks).  Haven enforces exclusive ownership: each IOMMU group
belongs to at most one partition.

**Invariant.**  Let `device_owner(d)` map a device to its partition.  Then:
```
∀ d1 d2 : Device,
  iommu_group(d1) = iommu_group(d2) →
  device_owner(d1) = device_owner(d2)
```
Violation at boot causes a panic; no runtime reconfiguration is permitted.

**C implementation:** `src/core/iommu/iommu_policy.c` — `hv_iommu_assign_device`,
`hv_iommu_check_group_exclusive`.

**Test coverage:** `tests/unit/test_iommu_policy.c`.

---

## Temporal Isolation

### IRQ Ownership (Interrupt Routing)

**Mechanism.**  GICv3 interrupt routing tables are programmed at boot to
deliver each physical IRQ exclusively to the virtual CPU interface of its
owning partition.  Software-generated interrupts (SGIs) from one partition
targeting another are blocked by Haven's EL2 trap handler.

**Configuration.**  The YAML config assigns each interrupt number to a
partition.  Haven validates that no IRQ is claimed by more than one partition.
At runtime, any attempt to inject an IRQ across partition boundaries triggers
a EL2 exception and is logged as a security event.

**Invariant.**
```
∀ irq : IRQNumber, ∃! p : Partition, irq_owner(irq) = p
```
(Every IRQ has exactly one owner; cross-partition injection is denied.)

**C implementation:** `src/core/irq/ownership.c` — `hv_irq_claim`,
`hv_irq_check_owner`.

**EL2 handler:** `src/core/exc/el2_exceptions.c` — intercepts SGI injection.

---

### Budget Scheduler (Temporal CPU Isolation)

**Mechanism.**  On cores shared between partitions, Haven implements a
work-conserving budget scheduler.  Each partition is assigned a `budget` (CPU
time in timer ticks) and a `period` (replenishment interval).  When a
partition's budget reaches zero, Haven preempts it and does not schedule it
again until the next period.

**Invariant (budget bound).**  At all times:
```
valid_budget(bs) ↔ bs.budget ≤ bs.period
```
This ensures no partition can consume more than its allocated fraction of CPU
time across any period.

**Preservation.**  The `consume` operation (deducting ticks on each preemption)
preserves the invariant:
```
valid_budget(bs) → valid_budget(consume(bs, ticks))
```

**C implementation:** `src/core/sched/budget.c` — `hv_budget_tick`,
`hv_sched_init`.

**Coq proof:** `verification/coq/BudgetScheduler.v` — `budget_leq_period` and
`consume_preserves_valid` theorems.

---

### Generic Timer (Period Enforcement)

**Mechanism.**  Haven programs the ARM64 Generic Timer's EL2 physical
timer (`CNTHP_TVAL_EL2` / `CNTHP_CTL_EL2`) to fire at each scheduler period
boundary.  On timer interrupt, Haven checks all partition budgets and
replenishes them if the period has elapsed.

**Invariant.**  Period replenishment is atomic with respect to partition
execution: a partition cannot observe a budget replenishment mid-slice.

**C implementation:** `src/core/time/timer.c` — `hv_timer_set_period`,
`hv_timer_handler`.

---

## Formal Properties Summary

| Invariant                        | C function(s)                          | Coq theorem                          | Isabelle theorem                    |
|----------------------------------|----------------------------------------|--------------------------------------|-------------------------------------|
| No PA overlap between partitions | `hv_stage2_map_partition`              | `spatial_isolation`                  | `spatial_no_overlap`                |
| Map operation preserves overlap-free | `hv_stage2_map_partition`          | `add_partition_preserves_isolation`  | —                                   |
| No shared PA (corollary)         | `hv_stage2_contains`                   | (corollary of `spatial_isolation`)   | `isolation_no_shared_pa`            |
| Budget ≤ period                  | `hv_sched_init`, `hv_budget_tick`      | `budget_leq_period`                  | —                                   |
| Consume preserves budget bound   | `hv_budget_tick`                       | `consume_preserves_valid`            | `consume_budget_preserves_valid`    |
| Zero-size region disjoint        | `hv_range_overlaps`                    | `zero_size_disjoint`                 | —                                   |

---

## Threat Model Mapping

The isolation model directly addresses the threats documented in
`docs/safety/THREAT_MODEL.md`:

| Threat                                    | Mitigating mechanism(s)                     |
|-------------------------------------------|---------------------------------------------|
| Guest reads another guest's physical RAM  | Stage-2 MMU (Layer 1)                       |
| Guest DMA to another guest's RAM          | SMMUv3 (Layer 2)                            |
| Guest hijacks shared peripheral           | IOMMU group ownership (Layer 3)             |
| Guest injects interrupt into another guest| IRQ ownership + EL2 trap (Layer 4)          |
| Greedy guest starves RTOS                 | Budget scheduler + timer (Layer 5)          |
| Guest modifies stage-2 tables             | EL2 privilege; stage-2 tables mapped RO in guest |
| Hypervisor bug allows partition escape    | Minimal TCB + formal proofs reduce attack surface |

---

## Invariants Maintenance Protocol

Haven's invariants are enforced at three points:

1. **Boot-time** — all invariants are checked during `hv_init()` before any
   partition is started.  A failed check causes an immediate panic; no
   partition is ever started in an invalid state.
2. **Static configuration only** — partition layout is never mutated after
   `hv_init()` completes.  There is no dynamic partition reconfiguration API.
3. **Exception handling** — EL2 synchronous exceptions from guests are
   inspected by `src/core/exc/el2_exceptions.c`; any operation that would
   violate an invariant (e.g., cross-partition SGI) is aborted and logged.

---

## Further Reading

- `docs/architecture/OVERVIEW.md` — system architecture and design principles
- `docs/safety/THREAT_MODEL.md` — full threat model
- `docs/safety/ASSUMPTIONS.md` — trusted assumptions (hardware correctness, firmware)
- `verification/coq/README.md` — Coq proof guide
- `verification/isabelle/README.md` — Isabelle/HOL cross-validation
