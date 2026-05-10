# Evaluation Plan

## Research questions

- Can memory/device isolation hold under deliberate violation attempts?
- Can RTOS latency remain bounded under Linux stress?

## Measurement strategy

- Latency distribution and worst-case response time.
- Deadline miss count for RTOS workloads.
- Isolation fault-injection pass/fail matrix.

## Bench setup

- QEMU baseline for reproducible pipeline checks.
- At least one real heterogeneous SoC for final validation.
