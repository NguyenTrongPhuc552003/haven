# i.MX95 Validation Runbook

Primary target board:
- NXP i.MX95 Dev Kit

Purpose:
- Provide a repeatable process for platform bring-up, isolation checks, and evidence capture.

## Prerequisites

1. Board firmware and boot chain documented.
2. UART console access available.
3. Known-good power and debug setup.
4. Candidate configuration checked into configs.

## Bring-up sequence

1. Validate memory map and reserved ranges.
2. Boot hypervisor image and confirm early logging.
3. Start Linux partition and verify baseline service health.
4. Start RTOS partition and verify deterministic task loop.

## Spatial isolation checks

1. Trigger cross-partition memory access attempts.
2. Trigger non-owner DMA attempts where test harness permits.
3. Verify unauthorized accesses are blocked and logged.

## Temporal isolation checks

1. Apply sustained CPU stress in Linux partition.
2. Measure RTOS response latency and deadline behavior.
3. Compare observed values against milestone thresholds.

## Evidence artifacts

1. Boot logs and partition launch logs.
2. Isolation test pass/fail report.
3. Timing metrics with measurement methodology.
4. Board configuration and software revision metadata.

## Exit criteria

1. No unauthorized cross-partition access observed.
2. RTOS latency stays within target bound for test window.
3. All collected evidence linked in chapter traceability.
