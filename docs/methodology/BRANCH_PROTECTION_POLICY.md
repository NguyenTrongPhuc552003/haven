# Branch Protection Policy

This policy defines how mainline quality gates are enforced during thesis development.

## Protected branch

Target branch:
- main

## Required status checks

1. ci / build-and-check (gcc)
2. ci / build-and-check (clang)
3. nightly-validation should remain green over time and must be triaged if red

## Merge controls

1. Require pull request before merge.
2. Require all required checks to pass.
3. Require branch to be up to date before merge.
4. Disable force pushes to protected branches.
5. Disable branch deletion for protected branches.

## Review controls

1. Minimum one review approval for normal changes.
2. Minimum two review approvals for core isolation-path changes.
3. Code owner review required for:
- src/core/mm
- src/core/irq
- src/core/sched
- include/haven

## Emergency policy

1. Hotfix merges are allowed only for build break or security fix.
2. Post-merge incident note must be added within one business day.
3. Missing tests/docs must be backfilled in the next patch.
