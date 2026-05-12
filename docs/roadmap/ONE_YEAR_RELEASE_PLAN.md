# One-Year Release Plan

This plan defines a 12-month execution strategy with one release every 3 months.

Thesis target:
- Static Partition Hypervisor for Asymmetric Multiprocessing.

Primary hardware:
- NXP i.MX95 Dev Kit.

Secondary validation platforms:
- QEMU arm64.
- Additional heterogeneous SoCs during Release 3 and Release 4.

Reference inputs:
- Omnivisor (ECRTS 2024) for partitioning and mixed-criticality validation mindset.
- NXP i.MX95 quick-start documentation for board bring-up boundaries and workflow.

## Program goals

1. Turn the current contract-level isolation primitives into a thesis-grade implementation.
2. Keep QEMU arm64 as the reproducible CI baseline and i.MX95 as the primary evidence target.
3. Make every thesis claim traceable to source code, tests, logs, and packaged evidence.
4. Prevent scope creep by treating platform bring-up, timing, and evidence packaging as first-class deliverables.

## Workstreams

1. Core isolation mechanisms
- Stage-2 memory partitioning.
- IRQ ownership and route denial.
- SMMU/DMA policy enforcement.
- Budget accounting and rollover semantics.

2. Platform control paths
- EL2 architecture glue.
- Timer and budget clock integration.
- Board-specific bring-up and partition launch flow.

3. Validation and evidence
- Unit and integration tests.
- QEMU reproducibility gates.
- i.MX95 campaign logs and metrics.
- Benchmark and traceability artifacts.

4. Thesis packaging
- Chapter traceability.
- Threat and assumption review.
- Reproducibility appendix.
- Final risk and limitation narrative.

## Release cadence

### Release 1 (Month 1-3): Foundation and deterministic engineering baseline

Objective:
- Freeze the implementation contract and establish the validation spine.

Implementation targets:
1. Freeze architecture contracts for memory, interrupt, and budget scheduler APIs.
2. Establish CI layers: PR CI, nightly CI, benchmark baseline jobs, evidence packaging.
3. Finalize repository governance: branch protection, CODEOWNERS, hooks, contribution policy.
4. Deliver QEMU-first repeatable integration flow tests.
5. Define measurement protocol and chapter traceability contract.

Exit criteria:
1. The public header contracts are stable enough to support downstream implementation.
2. `scripts/ci-preflight.sh` and `scripts/package-evidence.sh` are the baseline validation gates.
3. The thesis traceability matrix covers all planned chapters and deliverables.
4. QEMU validation is reproducible across repeated runs.

Primary artifacts:
- `include/haven/*.h`
- `tests/unit/*`
- `tests/integration/*`
- `docs/methodology/*`
- `docs/roadmap/*`

### Release 2 (Month 4-6): Isolation mechanisms implementation maturity

Objective:
- Move from contract-level models to robust isolation enforcement logic.

Implementation targets:
1. Implement stage-2 table management beyond stubs with partition boundary enforcement.
2. Implement interrupt ownership table and explicit deny policy transitions.
3. Implement budget accounting with period rollover policy and negative-path tests.
4. Add integration negative tests for invalid ownership, invalid map ranges, and budget overrun.
5. Publish first reproducible benchmark trends and regression criteria.

Exit criteria:
1. Invalid memory, IRQ, and budget inputs fail closed.
2. The negative test suite exercises the dominant isolation failure classes.
3. Benchmark baseline data exists for the main development toolchains.
4. The implementation path is still auditable and small enough for a thesis TCB story.

Primary artifacts:
- `src/core/mm/stage2.c`
- `src/core/irq/ownership.c`
- `src/core/sched/budget.c`
- `src/core/dma/smmu.c`
- `tests/integration/test_isolation_negative.c`

### Release 3 (Month 7-9): Platform validation and mixed-environment verification

Objective:
- Prove the isolation design on virtual and physical platforms.

Implementation targets:
1. Execute i.MX95 board bring-up runbook and collect evidence snapshots.
2. Add cross-OS virtual validation (macOS, Linux, Windows) for Docker/QEMU workflows.
3. Validate development virtualization paths:
- OrbStack on macOS.
- Docker engine on Linux.
- Docker Desktop or WSL2 path on Windows.
4. Add at least one secondary heterogeneous platform for cross-checking assumptions.
5. Expand CI artifact retention and milestone evidence comparison records.

Exit criteria:
1. QEMU and i.MX95 both produce repeatable validation outputs.
2. The evidence bundle includes logs, metrics, metadata, and config snapshots.
3. Cross-OS workflow notes are present and reproducible.
4. The thesis claim set is now backed by platform-specific results, not just code contracts.

Primary artifacts:
- `configs/arm64/qemu-virt.yaml`
- `configs/arm64/imx95-devkit.yaml`
- `docs/porting/IMX95_HARDWARE_SETUP.md`
- `docs/porting/IMX95_VALIDATION_RUNBOOK.md`
- `docs/methodology/IMX95_EVIDENCE_TEMPLATE.md`

### Release 4 (Month 10-12): Thesis evidence lock and submission-quality packaging

Objective:
- Freeze feature scope and package the implementation as a thesis-ready body of evidence.

Implementation targets:
1. Freeze feature scope and focus on reliability, documentation, and reproducibility.
2. Run complete validation matrix across virtual and physical targets.
3. Consolidate chapter traceability with objective evidence links.
4. Produce final performance, isolation, and risk analysis reports.
5. Prepare thesis appendices from repository artifacts and runbooks.

Exit criteria:
1. Spatial isolation validation: unauthorized memory and DMA access blocked.
2. Temporal isolation validation: measured RTOS latency remains bounded under Linux stress.
3. Reproducibility: independent rerun of virtual and board workflows succeeds.
4. Documentation: complete A-to-Z implementation, validation, and risk narrative.

Primary artifacts:
- `docs/thesis/*`
- `docs/safety/*`
- `docs/methodology/*`
- `build/evidence/*`
- `build/benchmarks/baseline.json`

## Monthly checkpoint model

1. Month 1:
- Freeze scope, claims, and milestone language.
- Establish the canonical validation commands.

2. Month 2:
- Harden the test suite around current contract-level behavior.
- Confirm the code paths that are still missing hardware wiring.

3. Month 3:
- Lock the Release 1 evidence bundle.
- Review the first traceability mapping against the thesis outline.

4. Month 4:
- Begin isolation hardening in the core modules.
- Expand denial-path tests.

5. Month 5:
- Close gaps in memory and IRQ policy handling.
- Record regression baselines.

6. Month 6:
- Lock Release 2 with negative-path coverage and benchmark trend data.

7. Month 7:
- Start platform wiring and QEMU end-to-end bring-up.

8. Month 8:
- Execute i.MX95 runbook steps and capture first board evidence.

9. Month 9:
- Add cross-OS validation notes and secondary platform cross-checks.

10. Month 10:
- Freeze features and run full matrix validation.

11. Month 11:
- Consolidate evidence, risk analysis, and thesis appendix material.

12. Month 12:
- Final rerun, final evidence pack, and thesis submission packaging.

## Quality gates per release

1. All CI jobs must pass on protected branch before release tag.
2. Required integration scenarios must pass on QEMU and primary board.
3. Evidence package must be generated and archived.
4. Safety assumptions and non-goals must be updated and reviewed.

## Exit criteria for final thesis completion

1. Spatial isolation validation: unauthorized memory and DMA access blocked.
2. Temporal isolation validation: measured RTOS latency remains bounded under Linux stress.
3. Reproducibility: independent rerun of virtual and board workflows succeeds.
4. Documentation: complete A-to-Z implementation, validation, and risk narrative.
