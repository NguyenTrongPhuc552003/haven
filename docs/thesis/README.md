# Thesis Documentation

This folder contains the master thesis working documents for:

**Title:** Static Partition Hypervisor for Asymmetric Multiprocessing:
Enforcing Spatial and Temporal Isolation Between Linux and RTOS on a Heterogeneous SoC

**Platform:** NXP i.MX95 Dev Kit (Cortex-A55 × 4 + Cortex-M7)

## Structure

| File | Purpose |
|---|---|
| `chapters/` | Chapter drafts and outlines |
| `figures/` | Diagrams and measurement plots |
| `references/` | BibTeX and annotated references |
| `measurements/` | Raw benchmark data and analysis scripts |

## Chapter outline

1. Introduction — motivation, problem statement, contributions
2. Background — hypervisor taxonomy, AMP SoC architecture, related work (Omnivisor, Jailhouse, Bao)
3. Haven Architecture — partition model, Stage-2 MMU, IRQ ownership, SMMU, budget scheduler
4. Implementation — i.MX95 bring-up, EL2 control path, FreeRTOS partition
5. Evaluation — spatial isolation validation, temporal isolation measurements, QEMU + hardware
6. Conclusion — contributions, limitations, future work

## Web portal

The live documentation portal at `https://haven-tau-eight.vercel.app/` mirrors
this content in browsable form, built from `website/src/content/docs/thesis/`.

## Key references

- Ottaviano et al., *The Omnivisor*, ECRTS 2024 — closest prior work
- NXP i.MX95 Reference Manual — primary hardware reference
- ARM Architecture Reference Manual (ARMv8-A) — EL2 / Stage-2 MMU specification
- ARM SMMU Architecture Specification (SMMUv3) — DMA isolation
