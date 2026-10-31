# Next bounded engineering decision, CP1 and prioritized backlog

Status: proposal for review. No source patch, coordinated implementation,
commit, publication or thesis-stage transition is performed by this document.
All three repositories remain required in the final artifact.

## CP1 necessity review

CP1 proposed cross-repository versioned guest profiles, report helpers and a
typed ELMOS preflight. Its integration requirements remain useful. They do not
all require code before preparation can close.

| Risk or requirement | Needed now? / evidence already available | Recommendation |
| --- | --- | --- |
| Wrong image, compiler, firmware, cwd or load address makes failure ownership unclear | Yes. Existing exact commands, image hashes, source diffs, independent manual reference and C1–C7 capture these inputs | Keep using the recorded inventory/checklist. No new schema/parser is needed to retire this immediate risk |
| KDAL accidentally treated as trusted EL2 code or every ioctl becomes a hypercall | Yes. Required-three-component scope and guest-placement/trust map explicitly prevent it | Retain the design constraint; no runtime ABI implementation |
| Missing Linux artifacts or unimplemented vGIC mistaken for a working guest environment | Yes. The guest input inventory and source findings mark these prerequisites explicitly | Report BLOCKED/NOT RUN. Do not turn absent inputs into a preflight/Linux feature project |
| New stable `haven-guest-profile/1` schema/generator and guest DT interface | Consumer and Linux support architecture not yet approved; entry fields and MMIO semantics are unsettled | Defer stable schema/generator code until a reviewed guest contract has a real producer/consumer. The version1 names in CP1 are proposals, not compatibility promises |
| New ELMOS integration CLI, build dispatch and typed firmware executor | A manual reference exists; full integrated manual path does not yet exist | Later automate the smallest demonstrated manual workflow; generic launch requirements remain design input |
| KDAL artifact report API and new UAPI test fixture | Useful when matched Linux/module inputs exist; K1 independently blocks initialization | Review K1 separately. Add only the test/report necessary for the selected independent functional baseline |
| Full Linux/vGIC/timer support | A major explicit runtime prerequisite, not validation of manifest fields | Separate M1 feature/design scope after the architecture decision; never include under CP1 preflight |

**Recommendation: defer coordinated CP1 implementation.** Preserve its draft,
manifest example and acceptance requirements for reuse. Its implementation-
first sequencing is superseded; required integration is unchanged. Reopen a
bounded subset when there is an approved stable producer/consumer contract
and a concrete manual step whose automation retires a demonstrated risk.
If argv/capture automation is needed earlier, review that ELMOS-only scope
against the existing manual TF-A scenario, without inventing a KDAL dependency.

## Next thesis decision: Stage1 scope, conditional on closure review

Stage0's exit review can accept the inventory with risks and authorize
formulating the Stage1 research question. It does not approve the answer.
Stage1 should produce one bounded problem/threat model that uses all three
components, compares relevant prior work/alternatives, distinguishes software
interfaces from device/DMA isolation, and states what evidence requires a
physical target. Answer WHAT/WHY/WHEN/WHERE/WHO/WHICH/HOW/HOW WELL before
approving dependent runtime features or quantitative thresholds.

The proposed Linux/KDAL placement remains a strong candidate, with its actual
implementation cost and missing vGIC support recorded. A closure decision need
not pre-approve that entire implementation. Board acquisition and feasibility
may bound the selected research claims; there is no physical support fallback
based solely on QEMU success.

## First concrete implementation proposal: EP-A2

**Title:** provide the compiler-generated memcpy ABI in ARM64 HAVEN.
**Classification/owner:** BUGFIX, HAVEN only.
**Reason now:** a real compiler-generated call fails to link under the
diagnostic strict build; the existing helper is implemented and target-tested.
This correction is useful regardless of the final guest/device architecture.

**Exact input:** HAVEN `f775304ce9443dc9861ba1880e4208c7ae19b314` plus the
preserved local work inventory. Apply only
[0001-arm64-export-memcpy.patch](../20261025-corrections/0001-arm64-export-memcpy.patch),
SHA-256 `d9c1b5189e7d3c15a89febf0ea30c56a4f8e7e394703544dbcf1e240b562dce2`.
The implementation delta is nine lines in `src/common/string.c`, adding an
ARM64-only standard-name wrapper forwarding to `hv_memcpy`. ELMOS/KDAL have
no source or interface change. Local diagnostics/formatting/script changes
must remain separate.

**Scope of the next approval:** apply this hunk and perform its focused
acceptance on an isolated build of the resulting tree. The existing reviewed
target experiment is reused unless relevant source/compiler/config changed
or a named uncovered case appears. Once this concrete scope is approved,
continue its application/validation without repeating the same permission
request. Commit/merge/publication and every larger correction are separate
actions; none is performed here.

| Acceptance | Current evidence / required action after approval |
| --- | --- |
| Correct failure and smallest fix | Existing pristine strict build fails specifically at `hv_stage2_map_partition`'s `memcpy`; patch fixes only that missing ABI entry |
| Patch identity and unrelated work | Verify hash/applicability and preserve current diff. Reject any bundled strict flag, PSCI, CPU-init, IRQ, timer or other memory-helper export |
| Actual-tree build and symbols | Fresh default ARM64 build and diagnostic strict build where their effective inputs changed; defined `memcpy`, no unresolved helper and no recursive helper cycle. Record exact commands/flags/ELF/hash |
| Semantic behavior | Existing native and on-target default/strict 4,160-case copy/canary/self-copy evidence satisfies the unchanged helper case. Broaden only for a concrete input/coverage change |
| Honest failure record | A1's initialized-data default-layout failure remains documented, separate from A2. No claim that boot, alignment or all GCC runtime requirements are fixed |
| Review scope | Existing scoped Isolation Guardian review supports this wrapper; re-review only a materially changed implementation/control-path scope. No new runtime interface |

The [A1/A2 decision](A1_A2_DECISION.md) records why adoption is recommended
independently of A1. Functional acceptance is already supported; explicit
adoption approval and actual-tree application checks remain. **The patch is
not applied by this task.**

## Prioritized backlog and estimates

Ranges below are engineering effort in person-days (approximately six focused
hours each), not calendar deadlines or measured runtime. Assume one engineer
familiar with ARM64/C/Go, the current working host/tools, accessible pinned
sources and no new procurement. Include focused validation/review preparation;
exclude waiting for reviews, downloads, board delivery and publication. Ranges
overlap in dependencies and must not be added as a committed project schedule.
The larger estimates are low-confidence planning allowances, to re-estimate
after the named decision/prototype.

| Priority / scope | Owner, dependency and exit | Effort range / assumptions | Hardware dependency |
| --- | --- | --- | --- |
| P0 EP-A2 ABI-only correction | HAVEN; concrete approval; exact hunk plus final-tree checks | **0.25–1 day**, existing implementation/evidence reused; grows only if source/tool inputs conflict | None |
| P0 Stage1 question/threat-model/architecture decision | HAVEN research owner with ELMOS/KDAL roles; closure review accepted before transition; approved scope/alternatives and measurement plan | **3–8 days**, focused prior-work/feasibility comparison; may require another bounded probe before decision | None for QEMU question; physical promises must remain conditional |
| P1 SC0 restricted bootstrap enforcement | HAVEN, separate scope after A2 review; all reachable pre-MMU C/asm/entry/phase limits enforced, stop before externally walked table enablement and regressions documented | **1–3 days**, pinned QEMU/TF-A bootstrap only; no claim that all current init/SMMU setup is covered, no page-table runtime, IRQ rewrite or full secondary startup | None |
| P1 K1 attach/probe association and unwind | KDAL, independent of HAVEN; success/failure cleanup and serialized unload tests | **0.5–2 days** code/focused tests once a matching Linux test build is available; environment preparation counted below | None |
| P1 independent Linux/KDAL artifact environment | ELMOS build tooling + KDAL module; verified source/config/cross compiler, direct Linux boot, compatible module/test artifacts | **2–6 days**, possibly **+2–5 days** for concrete macOS host-tool/kernel-version problems. No unchanged vendor script execution assumed | None |
| P2 Linux/vGIC feasibility design | HAVEN; approved candidate placement; trace actual pinned Linux accesses, virtual timer/console and fault semantics, then approve bounded M1 feature | **3–7 days**; includes decision/prototype specification, not completed Linux support | None |
| P2 M1 bounded UP Linux support implementation | HAVEN, approved M1 design plus runtime memory contract; PID1 with confined virtual frontend and negative tests | **15–40 days**, low confidence until register/MMIO trace and frontend scope are reviewed; no SMP/device DMA | None |
| P2 ELMOS firmware launch/capture subset | ELMOS only after a stable manual workflow and scope review; exact args/cwd/images and bounded lifecycle regressions | **2–5 days**, finite structured settings; no arbitrary command escape and no Linux/vGIC code | None |
| P2 deferred CP1 subset | All affected owners only when actual contract consumers stabilize; profile/artifact/preflight tests | **3–8 days** if coordinated schema/report implementation remains necessary; re-estimate or omit unneeded parts | None |
| P3 M3 integrated acceptance | ELMOS orchestration, HAVEN policy/probe, KDAL fixture; M1 and independent KDAL baseline pass | **3–8 days**, controlled fresh-VM positive/negative pair; failure diagnosis can enlarge range | None |
| P3 M4 initial physical board/device | HAVEN/KDAL/ELMOS; exact recoverable board/firmware/device selected from actual evidence | **10–30 days after acquisition/entry feasibility**, very low confidence. No finite completion estimate if EL2, recovery or device containment cannot be established | **BLOCKED: no boards/logs**; procurement/wait excluded |
| P3 M5 evaluation/evidence | All three; approved question/method and working selected mechanism | **10–25 days** for one bounded experiment campaign/analysis after prerequisites; thesis writing/review needs a separate estimate | Physical hardware required only for physical claims selected by the research decision |

The ordered next engineering decision is EP-A2, not simultaneous changes in
all three repositories. K1 and the research decision may proceed as independent
approved scopes; required final integration does not require synchronized edits
for unrelated defects. No item in this table is a new Stage0 closure blocker.
