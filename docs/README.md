# Haven documentation

Updated 2026-11-03. The thesis targets the **CIX P1 (Sky1) SoC only**; its
working research question is enforcement read-back (see the repository
[README](../README.md)). Documents fall into three groups.

## Current (maintained)

| Document | What it holds |
| - | - |
| [`methodology/stage0/20261025-corrections/FINDINGS.md`](methodology/stage0/20261025-corrections/FINDINGS.md) | Every known defect and tool gap, with status, evidence and the commit that resolved it |
| `methodology/stage0/` to `stage2/` | Dated experiment records: changes, expectations fixed before the run, raw runs (large dumps listed by hash), limits |
| [`methodology/stage2/20261102-readback-v4/`](methodology/stage2/20261102-readback-v4/README.md) | Read-back v4 and its grants (v5 adds the EL2 map check, see `el2-memory`) |
| [`methodology/stage2/20261102-a1-cases-v3/`](methodology/stage2/20261102-a1-cases-v3/RESULTS.md) | Frozen seeded-fault case set, random held-out faults, scores |
| [`methodology/stage2/20261102-partition-b/`](methodology/stage2/20261102-partition-b/README.md) | Two partitions on two CPUs, per-CPU interrupts, partition B's timer |
| [`methodology/stage2/20261102-secondary-cpu/`](methodology/stage2/20261102-secondary-cpu/README.md) | CPU map, entry checks, ready handshake for secondary CPUs |
| [`methodology/stage2/20261102-el2-memory/`](methodology/stage2/20261102-el2-memory/DECISION.md) | Haven's own EL2 memory contract: decision and results |
| [`methodology/stage2/20261102-budgets/`](methodology/stage2/20261102-budgets/README.md) | CPU time budgets enforced by the EL2 timer; QEMU timer lateness |
| [`methodology/stage2/20261102-interference/`](methodology/stage2/20261102-interference/README.md) | Plan for the interference measurements (board) and the QEMU pipeline check |
| [`research/related-work.md`](research/related-work.md) | Literature matrix for read-back and interference; novelty statement under review |
| [`porting/cix-p1-ledger.md`](porting/cix-p1-ledger.md) | CIX P1 platform facts and weaknesses: IORT, SMMUs, GPU, firmware, board differences |
| [`architecture/gpu-partition.md`](architecture/gpu-partition.md) | GPU partition design and the board experiments G1 to G4 |
| [`architecture/NAMING_RATIONALE.md`](architecture/NAMING_RATIONALE.md), [`architecture/REPOSITORY_STRUCTURE.md`](architecture/REPOSITORY_STRUCTURE.md) | Name and layout |
| [`contributing/`](contributing/) (testing guide, release process, review checklist), [`safety/STATIC_ANALYSIS_POLICY.md`](safety/STATIC_ANALYSIS_POLICY.md), [`safety/MISRA_DEVIATION_RECORD.md`](safety/MISRA_DEVIATION_RECORD.md) | Engineering process |

## Background (predates the current research question)

Written in May 2026 for the earlier "spatial and temporal isolation between
Linux and an RTOS" framing and not yet revised. Each carries a note at the
top. They will be rewritten or retired as the thesis chapters are written.

- Architecture: `architecture/OVERVIEW.md`, `ISOLATION_MODEL.md`, `THESIS_DEEP_DIVE.md`
- Safety: `safety/THREAT_MODEL.md`, `ASSUMPTIONS.md`
- Methodology: `EVALUATION_PLAN.md`, `CHAPTER_TRACEABILITY.md`, `BENCHMARK_BASELINE.md`,
  `CI_STRATEGY.md`, `CI_TROUBLESHOOTING.md`, `VIRTUAL_PLATFORM_VALIDATION.md`,
  `PHYSICAL_BOARD_VALIDATION.md`, `BRANCH_PROTECTION_POLICY.md`
- Porting and development: `porting/PORTING_GUIDE.md`,
  `porting/CROSS_OS_VIRTUALIZATION_RUNBOOK.md`, `contributing/DEVELOPMENT_GUIDE.md`

## Legacy (kept for history, not maintained)

From the earlier NXP i.MX95 plan and its month-based release roadmap. The
i.MX95 and i.MX8QM platform code and CMake presets are likewise kept but not
built or tested by CI.

- `porting/IMX95_HARDWARE_SETUP.md`, `IMX95_VALIDATION_RUNBOOK.md`,
  `IMX95_EVIDENCE_CAPTURE.md`, `methodology/IMX95_EVIDENCE_TEMPLATE.md`
- `roadmap/` (all files)
- `methodology/VERCEL_DEPLOYMENT.md` (the documentation portal is not
  deployed by the Codeberg CI)

## Evidence conventions

Records separate established fact, source-derived claims, experimental
observations, inferences and proposals. Expected outcomes are written before
a run and committed or hashed; verdicts on case sets come from the owner,
not from the tool's author. QEMU results never stand in for board results.
