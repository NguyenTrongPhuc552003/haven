# Stage 0 closure review and next engineering decision

Date: 2026-10-25. Classification: AUDIT / RESEARCH / DOCUMENTATION. **Recommend
close with recorded risks.** The user must accept/amend the closure decision;
Stage0 remains active and no transition is declared here.

All three engineering components remain required: ELMOS prepares/builds/
launches/tests/captures; HAVEN owns trusted isolation/admission/lifecycle;
KDAL supplies useful device abstraction inside the admitted boundary. Their
candidate integrated architecture is not an observed integrated runtime.

## Review package

| Artifact | Decision it supports |
| --- | --- |
| [EXIT_CHECKLIST.md](EXIT_CHECKLIST.md) | PASS/OPEN/BLOCKED/DEFERRED per criterion, exact evidence, uncertainty/consequence, owner/action and whether it prevents closure |
| [A1_A2_DECISION.md](A1_A2_DECISION.md) | Authoritative explanation of the captured alignment fault; strict bootstrap versus EL2 translation; assumptions/regression matrix; independent A2 adoption recommendation |
| [NEXT_DECISION.md](NEXT_DECISION.md) | CP1 necessity/deferral, Stage1 decision scope, prioritized effort ranges and EP-A2 concrete implementation proposal |
| [Updated engineering roadmap](../20261025-integration-design/ROADMAP.md) | Thesis-stage mapping distinct from M0–M5; original milestone criteria retained as delivery criteria |
| [Stage index](../../../../../stages/README.md), [workspace rules](../../../../../AGENTS.md) | Current navigation and stable preparation exit/reuse/approval rules |
| [repository-state.json](repository-state.json), [session-start-haven.patch](session-start-haven.patch), [review-sources.json](review-sources.json), [verification.json](verification.json) | Revisions, preserved changes, primary/reference source ledger and final integrity checks |

## Recommendations

Preparation has identified the required repositories and authority, pinned
host/virtual observations, reconstructed actual interfaces, preserved evidence,
and exposed the missing work with owners and bounded proposals. **Completing
M0, M1 Linux/vGIC, M3 integration, M4 physical validation or M5 evaluation is
not a Stage0 exit condition.** The four unavailable boards remain BLOCKED;
their missing EL2/device evidence cannot become a support claim.

For **A1**, recommend the smallest justified next contract: a pinned strict-
access primary bootstrap, ending before external table publication, secondary
release, guest execution or unreviewed shared/cached memory use. This is
proposed, not already enforced; it does not certify all of current `haven_init`.
The existing M=0/A=0 snapshots, unaligned LDR/STP fault instructions and matched
strict/default comparisons explain the captured failure when read against
Arm's Device-memory alignment rules. A strict flag is not a general memory,
atomic, cache or Linux-runtime initialization solution. Explicit EL2 Normal-
memory translation can be a later phase with its own justified requirements;
safe pre-MMU code remains necessary even then. No new experiment was needed.

For **A2**, recommend adoption of the existing nine-line memcpy ABI correction
after the exact-scope gate and final-tree checks. Its current build, symbol,
native and ARM64 evidence is sufficient for the narrow helper change. It need
not wait for all A1 engineering or Linux support. The known default-layout
failure remains an explicit independent risk. No production hunk is applied.

For **CP1**, defer coordinated schema/generator/CLI implementation. The current
input inventory, hashes, manual commands and failure-owner contracts meet the
immediate planning need. Stabilize the research/guest artifact consumers before
creating a new cross-repository configuration interface. Retain the original
proposal as design material; Linux/vGIC remains a separate future feature.

The next bounded engineering proposal is **EP-A2: apply only the identified
ARM64 memcpy entry and perform focused acceptance**, HAVEN only, estimated
0.25–1 person-day with current inputs. Separately, Stage1 must choose the
bounded research question/threat model/architecture; required KDAL inclusion
does not pre-approve its entire candidate runtime design.

## Superseded planning statements and retained history

| Earlier statement | Current interpretation |
| --- | --- |
| M0–M5 refine “Stage0 integration work” | Superseded in ROADMAP: these are engineering work packages spanning thesis stages, not preparation closure prerequisites |
| CP1 coordinated implementation should be the next work | Superseded recommendation: retain requirements, defer schema/CLI/generator implementation; review EP-A2 first |
| A1 ledger had no decisive M/TCR/MAIR/TTBR captures and A2 target execution was unverified | Historical at initial capture; subsequent runtime package supplies those readbacks and target execution. The new decision uses that evidence without rewriting the ledger |
| All A1 work must finish before any narrow A2 adoption | Superseded by independent ABI assessment, with A1 risk retained; explicit adoption gate still applies |
| KDAL optional or no integration design defined | Optional scope already superseded by user mandate; no implemented integration remains an accurate observation, with a candidate design now present |
| Baseline's old “Stage0 work remaining” list includes implementation/physical work | Read as historical engineering backlog. Current exit checklist is authoritative for this closure recommendation |

The four earlier evidence archives, source patch, logs, images and metadata
are unchanged. Only the roadmap/parent navigation receive current planning
notices. Before-state copies of edited documents are kept here as `.before`
provenance, not active instructions. Repository-local constraints remain intact.

The integration package's capture-time `package-manifest.json` is preserved.
Its README/ROADMAP hashes intentionally identify their original versions;
those exact versions are retained in this package's `.before` snapshots.
The two current planning notices are recorded in `documentation-changes.patch`
and this package's new manifest. No runtime archive or other old package member
changed. This distinguishes authorized planning edits from evidence corruption.

## Validation and limits

No builds or mechanism experiments were repeated. Work performed: current
Git/diff capture, relevant source/evidence reading, authoritative architecture
and versioned compiler review, focused spinlock-symbol reachability search,
preservation/hash and Markdown navigation checks. The Isolation Guardian
independently reviewed the contract/closure recommendations; its qualification
that SC0 is a proposed bounded phase was incorporated.

The root workspace has no Git repository. HAVEN/ELMOS/KDAL revisions and four
pre-existing tracked HAVEN changes remain unchanged; ELMOS/KDAL stay clean.
Manuscript PDF changes and wiki were preserved. This package and prior evidence
are local, Git-visible and uncommitted, with no remote backup performed.

Known carried risks are default HAVEN alignment/boot failures, incomplete
IRQ/timer/guest support, KDAL lifecycle and matched-kernel prerequisites,
uncertain physical feasibility, local-only evidence retention and an unapproved
research question. They affect later capability claims and implementation
decisions, not whether the preparation has discovered and owned them.

No production patch, coordinated source change, commit, publication, board
support claim, DMA/real-time guarantee or stage transition is made here.
