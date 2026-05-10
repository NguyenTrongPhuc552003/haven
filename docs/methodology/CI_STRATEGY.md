# Continuous Integration Strategy

This project uses a layered CI approach to support a one-year thesis timeline.

## CI layers

1. Pull request and push CI
- Compiler matrix: gcc and clang.
- Required gates: build, style, tests, config validation.
- Artifact outputs: static library and metadata for traceability.

2. Nightly validation
- Scheduled daily run for early regression detection.
- Same preflight suite as primary CI.
- Artifacts retained for longitudinal comparison.

## Why this matters for thesis delivery

1. Ensures reproducibility across toolchains.
2. Detects drift before milestone deadlines.
3. Preserves evidence continuity for chapter traceability.

## Operational policy

1. Failing CI blocks integration until fixed.
2. Nightly failures trigger next-day triage.
3. All significant fixes should include tests and docs updates.

## Board and platform alignment

1. QEMU arm64 is the mandatory reproducible baseline in CI.
2. i.MX95 validation evidence is tracked in milestone reports.
3. Secondary boards are optional unless promoted to milestone scope.
