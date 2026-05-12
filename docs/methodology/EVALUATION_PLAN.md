# Evaluation Plan

## Research questions

- Can memory/device isolation hold under deliberate violation attempts?
- Can RTOS latency remain bounded under Linux stress?

## Evaluation scope and platforms

- **Primary thesis platform:** NXP i.MX95 Dev Kit.
- **Reproducible CI baseline:** QEMU arm64 (`qemu-virt` config).
- **Secondary cross-check:** additional heterogeneous platform during later milestones.

## Measurement strategy (what to measure)

- Latency distribution and worst-case response time.
- Deadline miss count for RTOS workloads.
- Isolation fault-injection pass/fail matrix.
- Budget overrun detection and containment outcomes.

## Experimental procedure (how to measure)

1. Run configuration validation:
   - `./scripts/check-configs.sh`
2. Run correctness baseline:
   - `./scripts/test.sh`
3. Run benchmark collection:
   - `./scripts/benchmark-baseline.py`
4. Execute virtual smoke scenario:
   - `./scripts/qemu-smoke.sh`
5. Execute board validation workflow:
   - Follow `docs/porting/IMX95_VALIDATION_RUNBOOK.md`
6. Package and archive evidence:
   - `./scripts/package-evidence.sh`

## Acceptance criteria by isolation claim

### Spatial isolation acceptance
- Unauthorized memory mappings are rejected.
- Unauthorized DMA windows are rejected.
- Unauthorized IRQ ownership transfers are rejected.
- Evidence source: unit/integration negative tests plus board/QEMU validation logs.

### Temporal isolation acceptance
- Budget overrun is detected and denied.
- RTOS task latency/jitter remains within predefined threshold under Linux stress.
- Deadline miss trend remains within agreed regression bound across repeated runs.
- Evidence source: scheduler tests, benchmark outputs, and validation campaign logs.

## Reproducibility rules

1. Every campaign must record:
   - commit SHA
   - config file used
   - platform identity
   - test/benchmark command lines
2. Re-run policy:
   - minimum 3 runs per key scenario
   - report median and worst observed value
3. Regression handling:
   - if threshold violated, block milestone closure until root cause and mitigation are documented.

## Milestone linkage

- **R2/M3 focus:** mechanism-level negative-path coverage (memory/IRQ/DMA).
- **R3/M4-M5 focus:** timing measurements and board-backed evidence depth.
- **R4 focus:** full matrix rerun and thesis-ready evidence lock.
