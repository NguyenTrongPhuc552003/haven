# Bounded implementation roadmap and first coordinated proposal

Status: proposed engineering roadmap; no implementation or stage transition
approved. M0–M5 are delivery work packages that can span thesis stages. They
are not Stage 0 exit requirements. The earlier wording that they refined
“Stage 0 integration work” is superseded by this clarification. All three
components remain required in the final artifact.

The current [closure review](../20261025-closure/README.md) recommends closing
preparation with recorded risks and separately reviewing a small A2 correction.
M1 Linux/vGIC, M3 integration, M4 physical validation, M5 evaluation, and even
completion of M0 are not prerequisites for that closure. Existing milestone
exit criteria below apply when delivering that engineering milestone.

## Mapping to thesis stages

| Thesis stage                | Decision / evidence needed                                                                                          | Relation to engineering work                                                                                                                      |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0 - Preparation/feasibility | Inventories, reproducible observations, proposed boundaries, known risks, board constraints and concrete next scope | M0 risks/approaches and M1–M5 dependencies are identified; implementations need not be finished                                                   |
| 1 - Research decision       | Approved bounded question, threat model, alternatives, architecture and evaluation approach                         | Decide which M1/M2/M3 capabilities and eventual M4/M5 evidence are necessary; separately reviewed small M0/KDAL bugfixes may retire known defects |
| 2 - Minimal implementation  | Implement only the approved mechanism/slice                                                                         | Deliver relevant M0–M3 work; M4 bring-up only when feasible and approved                                                                          |
| 3 - Evaluation              | Controlled measurements of the chosen mechanism                                                                     | M5, using M3 and M4 where claims require them                                                                                                     |
| 4 - Thesis/release          | Reviewed traceability and approved release/publication                                                              | Consume supported milestone results; no milestone label authorizes publication                                                                    |

The physical branch can remain BLOCKED without preventing Stage 0 closure.
Actual research claims requiring it remain blocked until hardware evidence
exists. No implementation milestone is a substitute for the Stage 1 decision.

## Milestones

| Milestone                                                   | Affected owners and concrete deliverables                                                                                                                                                                                                                          | Dependencies / tests / exit criteria                                                                                                                                                                                                                                                                                                                                                                                                  | Deferred capabilities                                                                                                                                   |
| ----------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **M0 - HAVEN compiler/memory and boot baseline**            | HAVEN: separately reviewed A2 memcpy entry, A1 explicit EL2 execution/memory contract, primary-only boot profile and reproducible demo regression. ELMOS: tool/version and typed firmware launch groundwork.                                                       | New isolated runtime evidence validates the byte helper but default alignment-sensitive image still fails; see RUNTIME.md. Resolve/bound A1 before tracked runtime adoption. Build/link/object checks, both layout controls, original A allowed/denied path; profile omits CPU_ON cleanly. Exit: reviewed tracked baseline with exact recipes/hashes and no unexplained primary fault.                                                | Full secondary support, generalized firmware discovery, scheduler and physical targets.                                                                 |
| **M1 - Minimum Linux environment**                          | HAVEN: Linux Image entry/profile, restricted guest DT, bounded UP vGICv3 frontend and guest timer/console path, explicit denied-access semantics. ELMOS: pinned Linux toolchain/config/source lock, initramfs/PID1 build and independent direct Linux control.     | M0; Image placement/range/register/DT tests; counter/timer/GIC delivery plus spurious/unowned/unsupported access controls; direct Linux versus HAVEN boot comparison. Exit: CPU0 Linux reaches PID1 under HAVEN using only admitted resources, while excluded accesses fail as specified. No default broad GIC/MMIO passthrough.                                                                                                      | SMP/guest CPU_ON, general Linux platforms, hotplug, disks, networking, 9p, physical backend/DMA.                                                        |
| **M2 - KDAL functional baseline**                           | KDAL: separate K1 association/unwind correction, AArch64 UAPI fixture and module lifecycle tests. ELMOS: build module against exact M1 kernel, deterministic packaging and compatibility record.                                                                   | K1 and prepared Linux build; may proceed independently of HAVEN once direct Linux control exists. Verify required backend/example build settings. Positive ring byte/count/digest; negative selection/pointer/power; attach failure, fd-close/unload/reload. Exit: same module/UAPI fixture passes direct Linux control, packaging compatibility recorded.                                                                            | DSL/compiler extensions, accelerators, generic discovery, 32bit compat, concurrent device removal and physical device drivers.                          |
| **M3 - ELMOS-driven integrated positive/negative scenario** | ELMOS: structured prepare/build/launch/test/capture workflow and C1–C7 manifests. HAVEN: reviewed Linux profile and known-PC terminal denial fixture/oracle. KDAL: unchanged admitted module + smoke fixture from M2.                                              | M1 and M2; run ACCEPTANCE.md P0–P8 and the admitted/excluded scratch-page control pair in fresh VMs. Exit: full hash/command/checkpoint/process record, exact KDAL round trip and externally observed HAVEN CPU-store denial, with independent controls and first-failure ownership.                                                                                                                                                  | End-to-end physical I/O, DMA, multiple Linux guests, latency guarantees and live recovery.                                                              |
| **M4 - Select and validate physical device/board**          | HAVEN: choose one exact board/firmware only after recovery/EL2/topology evidence; admit one recoverable device and its MMIO/IRQ/DMA path. KDAL: useful backend for that admitted device. ELMOS: reproducible board build/serial capture and recovery recipe.       | M3 plus actual board possession, recovery and firmware evidence. First candidate class is non-DMA UART/byte-stream device if independently recoverable and separable from trusted debug; exact board/device remains unselected. Exit: allowed physical operation, unauthorized-operation denial and ownership/interrupt tests on the exact board; any DMA claim additionally requires requester/stream/SMMU allowed+denied transfers. | Other three boards, accelerators and DMA devices unless explicitly selected. No hardware available now: physical work is blocked, not nominally passed. |
| **M5 - Research evaluation and thesis evidence**            | All3: approved research question/threat model, selected workload and reproducible measurements. HAVEN supplies mechanisms, KDAL device abstraction workload, ELMOS controlled runs/raw data. haven-thesis receives reviewed claims; wiki only at approved release. | Approved Stage1 research decision and mechanism feasibility (M3; M4 for physical claims). Baselines, negative controls, interference workload if relevant, sample plan, timing method/uncertainty and reproducibility reruns. Exit: traceable claim→source→configuration→raw result with limitations.                                                                                                                                 | Unjustified novelty, inherited latency/TCB promises and unsupported board/real-time/DMA claims.                                                         |

M1 and M2 are not forced into an artificial serial dependency: Linux artifact
preparation and KDAL lifecycle validation can proceed independently of HAVEN's
virtual frontend. M3 joins their independently passing outputs. No new thesis
implementation repository is required; external Linux/firmware are pinned
dependencies managed by ELMOS.

## Existing findings: blockers for this slice

| Finding                                        | Required for CPU0 Linux/KDAL slice?                                     | Scope retained                                                                                                                                                                |
| ---------------------------------------------- | ----------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| A2 compiler ABI                                | Yes, M0; on-target byte checks now pass                                 | memcpy export only; no PSCI/IRQ/timer/flag changes bundled into it                                                                                                            |
| A1 alignment                                   | Yes, M0; default layout failure persists                                | Choose and review strict-access or initialized translation contract from evidence; current experiment is a diagnostic mitigation                                              |
| B1 firmware/conduit                            | Explicit primary-only launch required, M0                               | Skip secondary calls in the new profile; do not globally turn HVC into SMC. Full CPU_ON implementation can defer                                                              |
| B2/B3 secondary initialization/topology        | CPU2 and guest SMP defer                                                | Do not call the old CPU2 path from this profile. Validate the admitted CPU0 identity and use consistent single-CPU guest DT                                                   |
| I1 LR capacity/state                           | Required for chosen virtual IRQ path, M1                                | Separate capacity loop fix from per-CPU state/reset design                                                                                                                    |
| I2 physical dispatch/service                   | Required subset for guest timer/console, M1                             | Define current/lower EL service, source quiescing, spurious IDs and EOI; do not adopt the custom probe ISR wholesale                                                          |
| I3 ownership/injection                         | Required bounded source/target rules, M1                                | Source belongs to admitted guest/CPU before injection; vGIC frontend and timer source mapping are additional missing support, not solved by calling existing public injection |
| T1 private IRQ API                             | Local CPU0 timer enable/service required; generic CPU2 targeting defers | Explicitly constrain the local operation; existing diagnostic CPU2 failure is not a production timer caller                                                                   |
| T2 CNTHP CPU-bound scheduling                  | Defer for fixed CPU0 guest                                              | Keep host budget timer masked; Linux CNTV/PPI27 support is independently mandatory                                                                                            |
| K1 lifecycle and K2/K3/K4 compatibility/limits | Required independent KDAL baseline, M2                                  | Separate KDAL correction, matched build flags/ABI and serialized lifecycle; no patch to hide a HAVEN boot failure                                                             |

The proposed UP vGIC frontend, Linux DT/entry and fail-closed denied-access
behavior require scoped feature designs beyond the original bug ledger.
They cannot be labelled completed by the old one-LR or bare-metal tests.

## Current CP1 recommendation - defer coordinated implementation

The artifact/hash/input checklist and failure ownership are useful now and
already exist in the integration package. They retire the risk of confusing
tooling failures with HAVEN/KDAL failures without new coordinated code.
Versioned profile generators, stable schemas, new CLI configuration and
component report APIs can wait until the research/architecture decision and
the independent Linux/KDAL artifact path make their consumers concrete.

The [closure backlog and next proposal](../20261025-closure/NEXT_DECISION.md)
supersede the implementation-first sequencing of original CP1 below. Retain
CP1 as a design candidate. Reopen only the smallest proven manual workflow
that needs automation; Linux/vGIC implementation is never a preflight task.

## Original CP1 proposal - retained for review history, implementation deferred

**Title:** versioned guest artifact contract and typed integration preflight.
**Purpose:** make the three repositories agree on reproducible inputs and
evidence before implementing a runtime device interface. CP1 does not claim
Linux boot support; missing inputs produce an explicit BLOCKED result.
Implementation requires review approval. The proposed changes are:

| Repository / proposed paths                                                                                                           | Minimal change and interface                                                                                                                                                                                                                                                                                                                                                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| HAVEN: `configs/qemu-virt/linux-kdal-up.profile.json`, `tools/config-gen/` profile schema/checks, `tests/` profile fixtures           | Own `haven-guest-profile/1`: admitted CPU0, IPA↔PA ranges, image/DT entry requirements, only allowed virtual events, scratch exclusion and primary-only policy. Emit a host-readable admission report and guest DT input. This first change validates static configuration; it does not add an EL2 JSON parser or enable guest execution.                                                                                    |
| KDAL: `tests/userspace/` C fixture and artifact-report helper                                                                         | Own AArch64 UAPI smoke behavior and report exact UAPI/module/kernel identities, selected `uart0` backend, expected bytes/errno and result record version1. No HAVEN header import or hypercall. K1 correction remains a separate KDAL-only review/commit before running the fixture.                                                                                                                                         |
| ELMOS: typed integration config in `core/config/`, preflight/artifact orchestration in `core/domain/`, focused CLI registration/tests | Consume HAVEN's profile and KDAL's artifact requirements; resolve fixed component build kinds, validate paths/hashes/versions/kernel-module compatibility and emit `elmos-run-manifest/1`. Explicit firmware-mode launch settings follow the existing launch contract. No arbitrary executable, shell command, hooks or unvalidated extra args. CP1's default action is preflight/dry-run; actual runtime is gated on M0/M1. |

`proposal.json` accompanies this document as a concrete machine-readable design
example. It is intentionally marked proposed with unavailable artifact hashes;
a real preflight must reject those placeholders. It does not pretend that a
configuration parser or these paths already exist.

**Dependencies and sequencing:** review CP1 contracts first; implement profile
validation and artifact reports as component-owned pieces with fixture tests;
then ELMOS consumes them. A2/A1 and K1 remain independent corrections with their
own gates. Only after M0–M2 pass does a later approved launch step execute M3.
CP1 must not silently add the large Linux/vGIC implementation under a preflight
title.

**Acceptance:** positive fixture profiles produce one consistent guest map and
artifact plan; invalid schema/hash/ABI, overlap with EL2/canary, unsupported
CPU/device, missing kernel/module and dirty revision mismatch fail at the
owning input boundary without launching. Tests use paths with spaces and
untrusted metacharacters to confirm they stay data. KDAL request/count/errno
fixture behavior passes its independent Linux baseline once inputs exist.
ELMOS preserves existing Linux runner command behavior in focused regressions.
All component tests run independently and no circular runtime dependency is
introduced. No physical-board acceptance is attached to CP1; M4 owns it.

**Review decision requested:** approve or amend the CP1 artifact/preflight
contract and the proposed Linux guest placement, with each runtime correction
still separately gated. This task stops with this actionable proposal; no
coordinated implementation or tracked production patch is applied.
