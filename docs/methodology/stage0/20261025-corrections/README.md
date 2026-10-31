# Stage 0 correction review package

Date: 2026-10-25. Classification: AUDIT / DOCUMENTATION; bounded compiler
validation of an unapplied proposal. Owner: HAVEN. Thesis relevance: establish
reviewable mechanism corrections and failure ownership before selecting a
research question or integrating components.

## Deliverables

| Artifact | Responsibility |
| --- | --- |
| [FINDINGS.md](FINDINGS.md) | Ten findings covering PSCI/secondary boot, IRQ service/ownership, per-CPU PPI/CNTHP and compiler alignment/ABI; contracts, evidence, confidence, dependencies, minimal corrections and positive/negative checks |
| [REVIEW.md](REVIEW.md) | Dependency order, Isolation Guardian review, first proposed correction and exact acceptance status |
| [0001-arm64-export-memcpy.patch](0001-arm64-export-memcpy.patch) | Unapplied nine-line implementation proposal against the pinned HAVEN source; only the missing compiler `memcpy` entry |
| [ELMOS_LAUNCH_CONTRACT.md](ELMOS_LAUNCH_CONTRACT.md) | Minimum structured firmware launch, process/capture requirements, HAVEN reference settings and KDAL's undefined integration status |
| [validate_proposal.py](validate_proposal.py), [memcpy_fixture.c](memcpy_fixture.c) | Reproduce the compiler failure and verify the isolated proposal without changing the working implementation |
| [validation-summary.json](validation-summary.json), [validation-inventory.json](validation-inventory.json), [validation-evidence.tar.gz](validation-evidence.tar.gz) | Results, 448 file hashes, all exact commands/logs/objects/images/source export, and durable verified evidence |
| [repository-state.json](repository-state.json), [session-start-haven.patch](session-start-haven.patch), [reviewed-archive-members.json](reviewed-archive-members.json), [verification.json](verification.json) | Revisions, preserved local differences, inspected legacy patch identities and final preservation checks |

## Recovered context and current state

HAVEN HEAD remains `f775304ce9443dc9861ba1880e4208c7ae19b314`; ELMOS remains
`5ef5cd7fb974fca961e1af7f5a7f08ff47f70e95`; KDAL remains
`1ab0dce6dedada4162325ba2101db63214ed896c`. ELMOS/KDAL are clean. No new
implementation or cross-repository interface change was made by this review.

The baseline was initially read at workspace `STAGE0_BASELINE.md`. During this
audit it moved to [stages/STAGE0_BASELINE.md](../../../../../stages/STAGE0_BASELINE.md).
Workspace `AGENTS.md` still points to the former location; treat that as an
index conflict, not a changed stage or scope. The moved file's older
workspace-relative links also need a separate indexing pass. This package uses
the actual current location and appends only the newly established review
observations there.

Three additional local HAVEN diffs appeared after the initial status query:
formatting in `arch/arm64/cpu.c` and
`tests/integration/test_secondary_cpu_isolation.c`, and corrected CI script
paths in `scripts/evidence/package-evidence.sh`. The prior fatal-print change
also remains. Their authorship is not inferred. All four differences were
saved in this package and preserved. The unrelated manuscript PDFs and wiki
were untouched. The workspace root has no Git repository.

The [prior boundary package](../20261025-boundaries/README.md) is the source of
the runtime observations; its archives were rehashed and kept unchanged. No
legacy evidence was rewritten to match this review. In particular, the new
ledger qualifies the earlier “wrong-CPU PPI defect” wording: the captured
CPU2 call is diagnostic and no production caller currently enables PPI26.

## New validation evidence and limitations

The isolated runner exported pristine HAVEN at the pinned revision. A strict
build failed on the compiler-generated `memcpy` symbol without any diagnostic
boot/IRQ changes. Applying only the candidate wrapper in that export let both
default and strict builds complete. Symbols, relocations, disassembly and a
native copy fixture passed their stated criteria. Runtime alignment remains
open; no QEMU or physical experiment was run here.

Raw output was copied from `/private/tmp/haven-stage0-corrections-abi` into
ignored `haven/build/evidence/stage0-20261025-corrections/` and frozen in this
Git-visible archive. Every archived member was read back and compared to its
inventory hash. Archive SHA-256:
`555d163cb99dde29165aa5c1421efc2d270ed692f80a17350105138b690d9853`.
The temporary directory and ignored build output are not required for recovery.
No commit, remote backup, upload, merge or release was performed.

To reproduce use the runner command in [REVIEW.md](REVIEW.md) with a fresh
directory. It requires the pinned Git object, Python3 with tarfile's data
filter, CMake, Ninja, the recorded GNU AArch64 toolchain and native clang/nm.
It installs/downloads nothing. All commands have 120-second bounds. Archived
`commands.json`, `inputs.json` and version logs are authoritative for actual
argv, source export, compiler and environment. HAVEN embeds build date/time;
a new build can have different image hashes despite identical source inputs.

Official sources were checked for GCC's freestanding runtime and alignment
options, PSCI startup, versioned TF-A QEMU boot and QEMU loader behavior.
The original Arm Generic Timer PDF URL failed to fetch during this audit;
the official documentation-service search result identified the versioned
102379_0102_01 document and PPI26. The prior preserved audit remains the basis
for its fuller timer-source reading. No new memory-attribute rule was inferred
from inaccessible documentation.

## Review boundary

The scoped design review is complete. Patch1 is ready for **proposal review**;
its build criteria pass and its ARM64 runtime acceptance is pending. The
adoption gate is still closed. Approval can authorize an isolated runtime
check before any tracked change. Later PSCI, CPU-init, IRQ and timer corrections
each require their own evidence and review. ELMOS has a minimum launch contract
only; KDAL integration is undefined. Physical-board support, end-to-end
integration, DMA isolation and real-time guarantees remain unestablished.
