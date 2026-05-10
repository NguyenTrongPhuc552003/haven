# Chapter to Milestone Traceability

This matrix maps thesis chapters to repository deliverables and measurable outcomes.

## Matrix

| Thesis Chapter | Goal | Repository Deliverables | Validation Evidence |
| --- | --- | --- | --- |
| Chapter 1: Problem and Motivation | Define mixed-criticality isolation problem on heterogeneous SoCs | README.md, docs/architecture/OVERVIEW.md | Scope review and baseline architecture acceptance |
| Chapter 2: Related Work | Position against Jailhouse, Bao, and seL4 approaches | docs/architecture/ISOLATION_MODEL.md | Comparative requirements checklist |
| Chapter 3: System Design | Specify static partition model and EL2 enforcement boundaries | include/haven/*.h, docs/architecture/REPOSITORY_STRUCTURE.md | Interface review and design walkthrough |
| Chapter 4: Spatial Isolation | Enforce memory and DMA separation | src/core/mm/stage2.c, src/core/iommu/, tests/isolation/ | Memory access violation test results |
| Chapter 5: Temporal Isolation | Enforce CPU budget and interrupt ownership discipline | src/core/sched/budget.c, src/core/irq/ownership.c | Latency and budget accounting tests |
| Chapter 6: Implementation | Deliver platform-ready baseline for emulation and real SoC | configs/arm64/qemu-virt.yaml, configs/arm64/imx8qm-mek.yaml | Boot logs and configuration lint results |
| Chapter 7: Evaluation | Quantify isolation behavior under interference | docs/methodology/EVALUATION_PLAN.md, tests/integration/ | WCET/jitter/deadline metrics |
| Chapter 8: Conclusions and Future Work | Document limits and next verification depth | docs/safety/ASSUMPTIONS.md, docs/roadmap/MILESTONES.md | Gap analysis and roadmap sign-off |

## Acceptance Criteria by Isolation Dimension

Spatial isolation acceptance:
- Guest cannot map or access non-owned memory regions.
- DMA from non-owner partition is blocked by policy.
- Interrupt route leakage across forbidden domains is denied.

Temporal isolation acceptance:
- Budget accounting prevents sustained overrun beyond configured bound.
- RTOS partition response remains within evaluated threshold under Linux stress.
- Scheduler and timer bookkeeping remain monotonic and deterministic.
