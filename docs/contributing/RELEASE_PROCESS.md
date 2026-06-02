# Haven Release Process

**Project:** Haven — Static Partition Hypervisor for AMP  
**Applies to:** All releases from v0.6.0 onwards  
**Audience:** Maintainers with push access to `main`  
**Last updated:** 2025-01

---

## Overview

This document describes the end-to-end process for preparing, validating, and
tagging a Haven release. The release script (`scripts/release.sh`) automates
most steps, but the maintainer must perform several manual checks before running it.

A Haven release progresses through four stages:

```
1. Pre-release checklist  -->  2. Automated release script  -->  3. PR merge  -->  4. Post-release
```

---

## 1. Release Types

| Type | Description | Branch strategy | Version bump |
| ---- | ----------- | --------------- | ------------ |
| **Patch** | Bug fixes, documentation updates only | `fix/<topic>` → `main` | `0.6.x` → `0.6.x+1` |
| **Minor** | New features that do not break existing API or isolation guarantees | `feat/<topic>` → `main` | `0.6.x` → `0.7.0` |
| **Major** | Breaking changes, TCB interface changes, or new platform targets | `feat/<topic>` → `main` with Isolation Guardian review | `0.x.0` → `1.0.0` |
| **Thesis milestone** | Release tied to a thesis chapter claim | Milestone branches → `main` | As appropriate |

---

## 2. Pre-Release Checklist

Complete this checklist manually before running `scripts/release.sh`.

### 2.1 Code quality

```
[ ] All CI jobs green on the release commit (ci.yml, static-analysis.yml, benchmark.yml)
[ ] No open Blocker-class findings in static analysis (STATIC_ANALYSIS_POLICY.md)
[ ] All MISRA deviations documented in MISRA_DEVIATION_RECORD.md
[ ] Isolation Guardian sign-off obtained for any TCB changes (if applicable)
```

### 2.2 Version files

```
[ ] VERSION file updated to the new version string (no trailing whitespace or newline)
[ ] CMakeLists.txt project(VERSION ...) matches VERSION file
[ ] CHANGELOG.md has an entry for the new version with a date and summary
[ ] README.md version badge (if present) updated
```

### 2.3 Documentation

```
[ ] docs/roadmap/DESCRIPTION.md Part 9 tracker updated with completed milestone PRs
[ ] docs/roadmap/MILESTONES.md milestones table reflects current completion state
[ ] docs/methodology/CHAPTER_TRACEABILITY.md: all closed milestones have evidence paths
[ ] API documentation updated if any public symbols changed
```

### 2.4 Evidence

```
[ ] cmake --preset host-tests && cmake --build build-host passes cleanly
[ ] ctest --test-dir build-host --output-on-failure: 0 failures
[ ] QEMU smoke test passes: ./scripts/qemu/qemu-smoke.sh
[ ] build/benchmarks/*.json files are up to date (run benchmark suite)
[ ] build/evidence/summary.json present with current git SHA
```

---

## 3. Version Bump Procedure

### 3.1 Update VERSION

```bash
echo "0.7.0" > VERSION     # example: bumping to 0.7.0
```

### 3.2 Update CMakeLists.txt

```cmake
project(haven
  VERSION 0.7.0             # match VERSION file
  ...
)
```

### 3.3 Update CHANGELOG.md

Add a new section above the previous latest release:

```markdown
## [0.7.0] — YYYY-MM-DD

### Added
- <summary of new features>

### Fixed
- <summary of fixes>

### Changed
- <summary of changes>
```

### 3.4 Commit the version bump

```bash
git add VERSION CMakeLists.txt CHANGELOG.md
git commit --no-verify -m "chore(release): bump version to 0.7.0"
```

---

## 4. Running the Release Script

```bash
./scripts/release.sh [--tag] [--no-smoke] [--dry-run] [--dirty]
```

For a full release with tag:
```bash
./scripts/release.sh --tag
```

The script performs:
0. Em-dash cleanup (`scripts/dev/fix-emdash.sh`)
0b. Working tree cleanliness check (skip with `--dirty`)
1. Version consistency check (`VERSION` file vs existing git tags)
2. CI preflight gate (`scripts/ci/ci-preflight.sh`)
3. Benchmark regression check against `tests/benchmarks/latency-baseline.json`
4. QEMU smoke test (skip with `--no-smoke` or if `qemu-system-aarch64` not available)
5. Evidence pack written to `build/releases/<version>/` (skip artefacts with `--dry-run`)
6. Optional git tag (requires `--tag`)

If any step fails, the script exits non-zero. **Do not force-continue** past failures —
diagnose and fix before re-running.

### Release script flags

| Flag | Effect |
| ---- | ------ |
| `--tag` | Create and push a git tag after successful validation |
| `--no-smoke` | Skip the QEMU smoke test (for headless CI runners or missing cross-compiler) |
| `--dry-run` | Run all checks but do not write artefacts or create a tag |
| `--dirty` | Allow uncommitted changes (development use only — never for production releases) |

---

## 5. Git Tag Policy

All releases must be tagged with a signed or lightweight tag:

```bash
# Lightweight tag (default via release script)
git tag v0.7.0

# Signed tag (recommended for major releases)
git tag -s v0.7.0 -m "Haven v0.7.0"

# Push tag
git push origin v0.7.0
```

**Tag format:** `v<major>.<minor>.<patch>` (semver, prefixed with `v`).

Tags are **permanent** — do not delete or move a tag that has been pushed to `origin`.
If a release tag is incorrect, create a new patch version and tag.

---

## 6. Post-Release Actions

After the release tag is pushed:

```
[ ] Create a GitHub Release from the tag with CHANGELOG.md entry as body
[ ] Attach build/releases/<version>/ evidence archive to the GitHub Release
[ ] Update docs/roadmap/MILESTONES.md: set milestone as ✅ with release tag
[ ] Update website (website/src/) with new version if applicable
[ ] Announce on project channel / thesis supervisor (if milestone release)
```

### Evidence archive attachment

```bash
# The evidence archive is created by the release script at:
ls build/releases/<version>/

# Zip and attach to GitHub Release:
cd build/releases
zip -r haven-<version>-evidence.zip <version>/
gh release upload v<version> haven-<version>-evidence.zip
```

---

## 7. Hotfix Process

For critical fixes to a released version:

1. Branch from the release tag: `git checkout -b fix/critical-bug v0.7.0`
2. Apply the fix with a minimal commit
3. Run the pre-release checklist (Section 2)
4. Bump to `0.7.1` (Section 3)
5. Open a PR to `main` (do not cherry-pick; re-apply the fix to `main` separately)
6. After merge: tag `v0.7.1` and push
7. If the fix applies to `main` as well, open a separate PR

---

## 8. Rollback Policy

If a release is found to be defective after tagging:

1. **Do not** delete the tag — create a new patch release with the fix instead
2. Document the defect in CHANGELOG.md under a new `## [0.7.1]` section
3. If the defect is in an isolation-critical path, trigger an Isolation Guardian review
4. If thesis evidence was based on the defective version, record a known gap in
   `docs/methodology/CHAPTER_TRACEABILITY.md`

---

## 9. Release History Quick Reference

| Version | Date | Key changes | Tag |
| ------- | ---- | ----------- | --- |
| 0.1.0-dev | 2024-xx | Initial CI/CD, core contracts | v0.1.0-dev |
| 0.4.0 | 2024-xx | SMMU coverage, EL2 exceptions, benchmarks | v0.4.0 |
| 0.5.0 | 2024-xx | QEMU two-partition demo, IRQ injection | v0.5.0 |
| 0.6.0 | 2024-xx | Secondary CPU, FreeRTOS context, static analysis gate | v0.6.0 |
| 0.6.1 | 2025-xx | CMake migration, benchmark baseline, evidence CI | v0.6.1 |

*Update this table at each release.*
