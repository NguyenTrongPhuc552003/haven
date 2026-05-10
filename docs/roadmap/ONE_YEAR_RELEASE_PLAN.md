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

## Release cadence

Release 1 (Month 1-3): Foundation and deterministic engineering baseline
1. Freeze architecture contracts for memory, interrupt, and budget scheduler APIs.
2. Establish CI layers: PR CI, nightly CI, benchmark baseline jobs, evidence packaging.
3. Finalize repository governance: branch protection, CODEOWNERS, hooks, contribution policy.
4. Deliver QEMU-first repeatable integration flow tests.
5. Define measurement protocol and chapter traceability contract.

Release 2 (Month 4-6): Isolation mechanisms implementation maturity
1. Implement stage-2 table management beyond stubs with partition boundary enforcement.
2. Implement interrupt ownership table and explicit deny policy transitions.
3. Implement budget accounting with period rollover policy and negative-path tests.
4. Add integration negative tests for invalid ownership, invalid map ranges, and budget overrun.
5. Publish first reproducible benchmark trends and regression criteria.

Release 3 (Month 7-9): Platform validation and mixed-environment verification
1. Execute i.MX95 board bring-up runbook and collect evidence snapshots.
2. Add cross-OS virtual validation (macOS, Linux, Windows) for Docker/QEMU workflows.
3. Validate development virtualization paths:
- OrbStack on macOS.
- Docker engine on Linux.
- Docker Desktop or WSL2 path on Windows.
4. Add at least one secondary heterogeneous platform for cross-checking assumptions.
5. Expand CI artifact retention and milestone evidence comparison records.

Release 4 (Month 10-12): Thesis evidence lock and submission-quality packaging
1. Freeze feature scope and focus on reliability, documentation, and reproducibility.
2. Run complete validation matrix across virtual and physical targets.
3. Consolidate chapter traceability with objective evidence links.
4. Produce final performance, isolation, and risk analysis reports.
5. Prepare thesis appendices from repository artifacts and runbooks.

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
