# Release 3 (Month 7-9)

**Status: IN PROGRESS**

Theme:
- Platform validation depth and mixed-environment verification.

## Deliverables

1. QEMU two-partition end-to-end boot with UART evidence capture. ← PENDING (requires QEMU run)
2. Full i.MX95 validation campaign with repeatable evidence capture. ← PENDING (requires hardware)
3. Cross-OS virtual validation completion (macOS/Linux/Windows). ← PENDING
4. Secondary heterogeneous board baseline validation (i.MX8QM-MEK). ← PENDING
5. Linux guest kernel module and userspace CLI. ✓ (completed in R2)
6. Analysis tooling: `latency_analyzer.py`, `jitter_plot.py`, `evidence_report.py`. ✓ (completed in R2)
7. Platform memory maps: `src/platform/qemu-virt/memory.h`, `src/platform/imx95-devkit/memory.h`. ✓ (completed in R2)
8. i.MX95 evidence capture methodology: `docs/porting/IMX95_EVIDENCE_CAPTURE.md`. ✓ (completed in R2)

## Mandatory checks

1. Cross-OS workflow runs and produces pass/fail evidence.
2. Board-level runbook execution logs are archived.
3. Evidence templates are filled for each validation campaign.

## Exit criteria

1. Primary and secondary board evidence linked to traceability matrix.
2. Virtual and physical validation results are consistent with assumptions.
3. Residual risk list is updated with mitigation priorities.
4. QEMU boot log shows "HAVEN: Denied stage-2 violation" for cross-partition access.
5. i.MX95 UART evidence archive committed with SHA256 manifest.
6. RTOS deadline miss rate < 0.1 % under 100 % Linux CPU stress (measured).
