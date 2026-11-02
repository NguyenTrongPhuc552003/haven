# Porting Guide

> **Predates the current research question (2026-11-02).** Written before the A1
> read-back direction (2026-11-01) and not yet revised: statements here may not match
> the code. Current state: [docs/README.md](../README.md).

Porting checklist:
- CPU topology and affinity mapping.
- Interrupt controller integration.
- Timer source and calibration.
- UART and early boot logging.
- Memory map and reserved regions.
- IOMMU/SMMU configuration where available.
