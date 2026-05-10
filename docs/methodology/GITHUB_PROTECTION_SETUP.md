# GitHub Protection Setup

Use this checklist to apply repository settings that match the project policy.

## Branch protection setup

Target: main

1. Open repository settings.
2. Go to Branches.
3. Add a branch protection rule for main.
4. Enable pull request requirement.
5. Enable required status checks.
6. Enable required up-to-date branches before merge.
7. Disable force push.
8. Disable branch deletion.

## Required status checks

1. ci / build-and-check (gcc)
2. ci / build-and-check (clang)

## Review policy

1. Require at least one approving review for general changes.
2. Require code owner review for critical paths.

## CODEOWNERS prerequisite

Ensure .github/CODEOWNERS exists and maps critical directories.

## Verification

1. Create a test PR with a workflow change.
2. Confirm checks must pass before merge is available.
3. Confirm code owner review is required for isolation paths.
