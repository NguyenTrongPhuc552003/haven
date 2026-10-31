# Required ELMOS–HAVEN–KDAL artifact: Stage 0 design package

Date: 2026-10-25. Classification: AUDIT / RESEARCH / DOCUMENTATION / bounded
EXPERIMENT. All three repositories are required in the final thesis artifact.
The user authorized this scope update and integrated design, while preserving
the coordinated implementation and tracked adoption gates. This package stops
at those gates with a concrete proposal.

**Subsequent planning clarification:** the [Stage 0 closure review](../20261025-closure/README.md)
separates thesis stages from M0–M5 and recommends deferring coordinated CP1
implementation. The original design remains a candidate; the updated roadmap
and closure recommendation supersede its implementation-first sequencing.
Runtime evidence and architecture observations below retain their original
scope. No stage transition or implementation approval is implied.

## Deliverables and authority

| Read                                                                                                                                                                                       | Content                                                                                                                                     |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------- |
| [Current workspace instructions](../../../../../AGENTS.md), [stage index](../../../../../stages/README.md), [baseline](../../../../../stages/STAGE0_BASELINE.md)                           | Durable required-three-component scope and repaired baseline navigation                                                                     |
| [ARCHITECTURE.md](ARCHITECTURE.md)                                                                                                                                                         | Observed versus proposed architecture, placement comparison, dependency map, actual Linux and KDAL prerequisites                            |
| [CONTRACTS.md](CONTRACTS.md)                                                                                                                                                               | Seven producer/consumer contracts: versions, trust/admission, resource needs, lifecycle, tests, evidence and first-failure ownership        |
| [ACCEPTANCE.md](ACCEPTANCE.md)                                                                                                                                                             | One pinned software-backend positive/API-negative and actual HAVEN stage-2 negative scenario, including allowed control and external oracle |
| [ROADMAP.md](ROADMAP.md), [proposal.json](proposal.json)                                                                                                                                   | M0–M5, finding dependencies/deferrals, and CP1: versioned image/profile contracts plus typed ELMOS preflight and KDAL fixture               |
| [RUNTIME.md](RUNTIME.md), [runtime-summary.json](runtime-summary.json), [register-summary.json](register-summary.json)                                                                     | Newly executed isolated ARM64 memcpy/alignment observations and limits                                                                      |
| [runtime-inventory.json](runtime-inventory.json), [runtime-evidence.tar.gz](runtime-evidence.tar.gz), [runtime_validation.py](runtime_validation.py)                                       | Verified durable reproduction inputs, patches, commands, configurations, images, logs and termination                                       |
| [repository-state.json](repository-state.json), [source-ledger.json](source-ledger.json), [guest-input-inventory.json](guest-input-inventory.json), [verification.json](verification.json) | Exact revisions/local changes, inspected source hashes, absent guest artifacts and final preservation checks                                |

This package supersedes earlier statements that KDAL was optional for the final
artifact. It does not alter the historical fact that no HAVEN/KDAL runtime
relationship was observed. The prior [boundary evidence](../20261025-boundaries/README.md)
and [correction ledger](../20261025-corrections/FINDINGS.md) remain intact; this
package records subsequent scope decisions and runtime observations.

## Decision proposed

Place KDAL in one UP AArch64 Linux guest under HAVEN. ELMOS prepares/builds
and launches the complete artifact bundle and captures checkpoints. KDAL's
existing `uart0` software ring supplies a useful initial device abstraction;
ordinary I/O stays in admitted guest RAM. HAVEN owns memory/CPU/device admission
and enforces the outer resource boundary. KDAL and Linux stay outside trusted
EL2; no per-operation hypercall or circular repository dependency is needed.

The integrated acceptance sequence is firmware/HAVEN entry → Linux/PID1 →
KDAL module initialization → exact 32-byte device round trip → API error controls
→ one known guest-EL1 unauthorized store blocked at stage-2, with an external
canary/fault oracle. The last negative is distinct from invalid KDAL device
selection, which is not authorization. The software device has no DMA.

The slice requires missing Linux entry, a restricted guest DT and bounded
virtual GIC/timer support. It does not inherit those capabilities from the
existing bare-metal demos. Source inspection also finds KDAL's probe-visible
driver association missing before probe, independently blocking module readiness.
M1/M2 must pass independent baselines before M3 can run.

## Current state and preservation

Revisions remain HAVEN `f775304ce9443dc9861ba1880e4208c7ae19b314`, ELMOS
`5ef5cd7fb974fca961e1af7f5a7f08ff47f70e95`, KDAL
`1ab0dce6dedada4162325ba2101db63214ed896c`. ELMOS and KDAL are clean. HAVEN's
four existing tracked diffs-CPU/test formatting, evidence script paths and the
fatal-print diagnostic-are saved in [session-start-haven.patch](session-start-haven.patch)
and preserved. Manuscript PDF changes and wiki are untouched. The workspace
root is not a Git repository. No new tracked implementation hunk was applied.

Authorized documentation changes: workspace AGENTS now requires all three
roles; its baseline reference points to `stages/`; the baseline's moved links
and the data index's baseline link are repaired; the stage index identifies
the new design and historical records. The data collection was inspected
read-only except that explicitly authorized index-link repair. Before-state
copies of the three edited existing documents are retained as `.before`
provenance files here; they are not active instructions.

The isolated runtime campaign has five expected verdicts: two reproduce the
alignment failure, one strict diagnostic layout reaches its pass condition,
and two run the memcpy semantic fixture successfully on ARM64. No Linux guest,
KDAL module, ELMOS integrated launch or physical board was executed. The default
layout fault persists, so the A1 contract and tracked adoption remain open.

All 1,360 new runtime files are frozen outside ignored builds and hash-verified;
the old evidence and memcpy proposal retain their recorded hashes. Existing
local toolchains and preserved firmware supplied the experiment. The missing
Linux build/Image/module/config inputs are listed precisely in the inventory;
no download, vendor execution, kernel build or package installation was needed.

## Review gate

CP1 is the first coordinated implementation proposal: HAVEN owns a versioned
static guest admission profile/report, KDAL owns an AArch64 UAPI fixture and
artifact requirements, ELMOS consumes both through structured build/launch
preflight and evidence formats. Exact proposed paths, interfaces, sequence and
positive/negative acceptance are in ROADMAP.md. K1 and the existing HAVEN
compiler/memory corrections stay separate reviews. The Linux/vGIC runtime
work is a later explicit M1 feature scope, not hidden inside CP1.

Approve or amend that proposal before coordinated implementation. KDAL's
inclusion is already decided; the remaining decision concerns this placement,
contract and bounded implementation scope. No integrated, board, DMA or
real-time result is claimed.
