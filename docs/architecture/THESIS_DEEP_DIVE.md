# Thesis Deep Dive

Title:
Static Partition Hypervisor for Asymmetric Multiprocessing: Enforcing Spatial and Temporal Isolation Between Linux and RTOS on a Heterogeneous SoC

## Why this thesis matters

Modern heterogeneous SoCs combine cores with different execution models in one chip:
- Cortex-A class cores for rich operating systems such as Linux.
- Cortex-M or Cortex-R class cores for deterministic RTOS workloads.

Without hard isolation, faults or compromise in one domain can corrupt memory,
steal device access, or introduce unbounded latency in safety-critical control paths.

## Static partition hypervisor rationale

Haven uses static partitioning to lock resource ownership before runtime:
- Partition boundaries are fixed at configuration/build time.
- CPU, memory, interrupt, and DMA ownership are explicit and immutable during mission mode.

Compared with dynamic virtualization models, static partitioning improves:
- Determinism under load.
- Analysis tractability.
- Certification suitability for standards such as IEC 61508 and ISO 26262.

## Isolation properties

### Spatial isolation

Goal:
Partition A cannot read, write, or DMA into partition B resources.

Mechanisms:
- Stage-2 page-table controls for CPU memory accesses.
- SMMU/IOMMU policy for DMA-capable peripherals.
- Interrupt ownership model with deny-by-default routing.

### Temporal isolation

Goal:
Overload in one partition does not violate bounded response in another.

Mechanisms:
- Budget-based scheduling with fixed period and budget windows.
- Core pinning for high-criticality domains when feasible.
- Controlled interrupt delivery and accounting at the hypervisor layer.

## Research contribution targets

1. A minimal EL2 hypervisor architecture for AMP mixed-criticality deployment.
2. Enforceable partition configuration model for memory/device/time ownership.
3. Reproducible evaluation flow that measures isolation under stress.
4. Engineering baseline suitable for transition toward formal and safety evidence.

## Evaluation direction

Primary measurements:
- Cross-partition memory and DMA violation resistance.
- Interrupt ownership correctness under adversarial stimuli.
- Worst-case latency and jitter of RTOS tasks while Linux is stressed.
- Budget overrun detection and containment behavior.

Recommended reference projects for comparative study:
- Jailhouse (Siemens)
- Bao Hypervisor (University of Minho)
- seL4 (formal verification reference)
