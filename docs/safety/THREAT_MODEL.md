# Threat Model

This document defines the adversary model and security goals for Haven.
All isolation claims in the thesis are scoped to this model.

## Adversary model

### Who the adversary is

1. A compromised or faulty partition (Linux-class or RTOS-class).
2. A compromised device driver within a partition that has DMA capability.
3. A guest that has escaped its EL1/EL0 privilege boundary.

### What the adversary can do

1. Issue arbitrary load/store instructions within its own EL1/EL0 context.
2. Program a device under its control to perform DMA to any bus address.
3. Trigger high CPU load (scheduling pressure) indefinitely within its domain.
4. Send malformed hypercall arguments (invalid partition IDs, addresses,
   sizes, access flags).
5. Attempt to acquire IRQs, IOMMU groups, or SMMU streams owned by
   another partition.
6. Attempt to read or write physical memory outside its Stage-2 allocation.

### What the adversary cannot do (trusted base)

1. Execute at EL2 privilege.
2. Modify Stage-2 page table descriptors directly.
3. Reprogram SMMU context or stream table entries directly.
4. Modify GIC distributor routing registers directly.
5. Alter the hypervisor binary or its static partition configuration.

## Security goals

### Spatial isolation

1. **Memory containment**: a partition cannot read or write physical memory
   outside its Stage-2 IPA→PA mapping.
2. **DMA containment**: a device StreamID owned by partition P can only
   issue DMA to physical addresses within P's Stage-2 allocation
   (`hv_smmu_configure_dma_window` enforces PA containment via
   `hv_stage2_partition_contains_pa`).
3. **IOMMU group exclusivity**: each IOMMU group is owned by exactly one
   partition; reassignment without release is denied.

### Temporal isolation

4. **CPU budget enforcement**: a partition cannot consume more than its
   configured `budget_ns` within a `period_ns` window.
5. **Deadline non-interference**: expiry acknowledgement for partition A's
   timer does not affect partition B's deadline state.
6. **IRQ route exclusivity**: an IRQ can be routed to exactly one partition;
   preemption or re-routing by another partition is denied.

### Fail-closed invariants

7. Every enforcement function returns a defined error code (`HV_EINVAL`,
   `HV_EPERM`, `HV_ENOSPC`) on invalid input - no silent success.
8. A partition with an expired, unacknowledged timer deadline cannot arm a
   new deadline (fail-closed guard prevents deadline bypass).
9. Partition ID 0 is reserved and rejected by all enforcement modules.

## Attack scenarios and mitigations

| Attack                                  | Module        | Mitigation                         | Test evidence                  |
| --------------------------------------- | ------------- | ---------------------------------- | ------------------------------ |
| Guest maps peer's PA                    | Stage-2       | Cross-partition PA overlap denial  | `test_spatial_isolation.c`     |
| Device DMA escapes to peer PA           | SMMU          | PA containment check vs Stage-2    | `test_smmu_hardware.c` (S1–S5)  |
| DMA window type mismatch (RO vs RW)     | SMMU          | Access-flag enforcement (S6/S7)    | `test_smmu_hardware.c` (S6–S7)  |
| StreamID pool exhaustion DoS            | SMMU          | Hard cap HV_ENOSPC (S8)            | `test_smmu_hardware.c` (S8)    |
| Guest steals peer's IRQ                 | IRQ ownership | Owner check + EPERM                | `test_isolation_negative.c`    |
| Guest steals peer's IOMMU group         | IOMMU         | Owner check + EPERM                | `test_isolation_negative.c`    |
| Guest exhausts CPU budget across period | Budget        | `hv_budget_consume` deny           | `test_temporal_isolation.c`    |
| Guest bypasses timer deadline           | Timer         | Fail-closed unack guard            | `test_timer.c`                 |
| Hypervisor state corruption via re-init | All modules   | `selftest_reinit` invariant        | `test_hypervisor_invariants.c` |
| Malformed hypercall arguments           | All modules   | Boundary validation at every entry | All unit test negative paths   |
| Partition B UART MMIO access            | Stage-2       | No UART map for B; stage-2 DABT    | `test_spatial_isolation.c`     |
| EL2 virtual IRQ injection without owner| IRQ ownership | Ownership validated before ICH_LR  | `test_el2_exceptions.c`        |

## Non-goals

1. Defense against microarchitectural side-channels (cache timing, Spectre).
2. Runtime dynamic partition migration or budget renegotiation.
3. Fault-tolerant recovery from hardware errors inside a partition.
4. Resistance to physically present attackers with JTAG or bus access.
5. Denial-of-service from a partition consuming its full allocated budget
   (budget exhaustion is permitted - it is the partition's own resource).
