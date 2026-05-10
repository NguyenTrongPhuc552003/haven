# CI Troubleshooting Guide

This guide provides quick triage steps for CI and nightly failures.

## Fast triage

1. Identify failing job and compiler.
2. Reproduce locally with the same compiler:
- CC=gcc ./scripts/ci-preflight.sh
- CC=clang ./scripts/ci-preflight.sh
3. Inspect generated metadata in build/ci/metadata.txt.

## Frequent failure classes

1. Build failure
- Verify header include paths and compile flags.
- Confirm no unsupported compiler-specific constructs were introduced.

2. Test failure
- Re-run ./scripts/test.sh locally.
- Check interface contracts and input validation behavior.

3. Config check failure
- Re-run ./scripts/check-configs.sh.
- Confirm required keys and timing budget bounds.

4. Nightly artifact gap
- Confirm build outputs exist before upload step.
- Validate artifact paths in workflow YAML.

## Escalation path

1. Open incident issue with failing run URL and commit hash.
2. Assign owner and expected fix window.
3. Add postmortem note for recurring failures.
