# Isolation Model

## Spatial isolation

Mechanisms:
- Stage-2 translation for CPU memory isolation.
- IOMMU/SMMU policies for DMA-capable peripherals.
- Static peripheral ownership with deny-by-default policy.

## Temporal isolation

Mechanisms:
- Dedicated core partitioning where feasible.
- Budget-based scheduling for shared cores.
- Interrupt routing and masking rules to prevent cross-domain starvation.
