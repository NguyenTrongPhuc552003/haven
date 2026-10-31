# Stage 1 candidate questions and architecture decision

Status: two bounded candidates, no final research-question approval. All three
repositories are required. **Provisionally recommend A for a cheap rejection
study before choosing a runtime implementation.** Novelty remains unestablished.
The [primary-source screen](PRIMARY_SOURCES.md) constrains the claims; the
[proposed experiment](NEXT_EXPERIMENT.md) can reject A without Linux/vGIC.

## Common evidence and trust assumptions

Observed: ELMOS has host/build/Linux-QEMU workflows; HAVEN has bare-metal
stage-2 and disposable IRQ/timer evidence; KDAL has a Linux module/UAPI and
software-ring backend, with a source-found initialization defect. There is no
observed integrated Linux/KDAL guest. The first ring round trip would establish
interfaces/lifecycle in admitted guest RAM, not a new device-isolation,
revocation, DMA or real-time result.

For both candidates, trusted enforcement includes HAVEN's relevant portable
policy **and its architecture/driver binding**, firmware and the tested platform
assumptions. Host build tools, operator policy and ELMOS experiment/oracle are
trusted inputs to the experiment, but not guest runtime services. A policy proof
alone cannot remove the hardware binding from the security argument.

The Linux/KDAL guest may be compromised and attempt arbitrary CPU resource
accesses or lie about requirements/readiness. A backend declaration or a guest
“quiesced” acknowledgment confers no authority. HAVEN independently admits
resources. Ordinary operations within that envelope need no per-operation
EL2 call. Side channels, physical attack, compromised firmware/toolchain and
availability/timing guarantees are excluded from the initial security claim.
DMA is excluded until a real initiator/path and deny test are established.

## A - Consistency from backend demand to actual enforcement

**Problem and prior work.** Multiple representations can disagree: KDAL's
backend requirements, trusted resource admission, guest description and actual
stage-2/IRQ binding. Static descriptors, verified initialization and verified
configuration-to-rights mapping already exist (P1–P4). The unresolved local
question is whether an independently checked consistency method and test oracle
find meaningful binding errors beyond ordinary descriptor validation. This is
not a proposal to call a new manifest format novel.

**Falsifiable question.** For a fixed non-DMA backend and defined operation
set, can cross-boundary semantic checks plus independent enforcement probes
detect more seeded, security-relevant representation/binding errors than
existing HAVEN policy tests plus structural validation, while accepting all
valid controls and preserving the guest's useful operation?

**Hypothesis and contribution.** On a frozen fault taxonomy, the method detects
at least one independently justified non-structural error missed by the strong
baseline, and the later enforced access outcome agrees with the trusted oracle.
Any unauthorized successful operation is a failure. Potential contribution:
a small consistency relation, fault taxonomy, independent negative-test method
and measured limits of resource abstraction/enforcement correspondence. No
formal verification, first-of-kind configuration system or absolute coverage
claim is made. Discovering only an existing known defect or comparing against
a deliberately weak baseline does not satisfy the contribution.

**Boundary and architecture.** Let E be the operator-approved resource envelope,
D the untrusted backend demand, G the admitted grant and H the independently
observed effective binding. Check D fits E, G is no broader than E, and H is
consistent with the permitted grant. A demand cannot enlarge E. A trace of
known valid operations checks usefulness; excluded-operation probes check
denial. Guest DT omission, hashes and guest logs are insufficient enforcement
oracles. The guest/EL2 resource boundary remains HAVEN's responsibility.

**Useful roles.** KDAL supplies a real operation/backend and its bounded
requirements/test vectors. HAVEN supplies admission, effective resource
bindings and failure behavior. ELMOS pins all representations, injects faults,
builds matched controls and captures an independent oracle. Every role is
necessary to answer the question, beyond merely booting three projects.
The current ring supplies a zero-external-resource control; a resource-bearing
backend is required for any final device-boundary claim. Initially target
non-DMA CPU/MMIO and admitted interrupt scope; select an actual safe device
only after its confinement granularity is known.

**Mechanisms and cost.** First host-only study:1–2 person-days, using literal
fixtures/current portable policies and no production schema. If it survives,
estimate3–8 days for the bounded consistency/probe mechanism,2–6 for a useful
KDAL backend/fixture,2–5 for ELMOS campaign support, plus the shared Linux/
virtual-frontend baseline (earlier15–40-day low-confidence estimate). These
are planning ranges, not approval or cumulative deadlines. A1, K1 and actual
Linux/IRQ support remain separate prerequisites for an eventual integrated run.

**Alternatives and measurements.** Compare (i) current policy/unit checks and
careful manual resource configuration, (ii) structural/range/version/hash
validation with those same policy checks, and (iii) the proposed semantic/
enforcement check at equal test inputs. Use P2/P3/P4 as design alternatives,
without claiming performance comparisons to systems not run. Measure fault
coverage by class, false rejection, oracle disagreement, useful-operation
success and reproduction failures. Separate known-bug regressions from scored
holdout cases. Negative controls include wrong owner, mapping/permission drift,
stale resource representation and unsupported granularity; the ring's lack of
MMIO/IRQ/DMA must never be padded with fictitious requirements.

**QEMU versus hardware.** Host results show model/check feasibility only. QEMU
can later exercise real HAVEN CPU stage-2 access/interrupt paths against the
chosen emulator map and external oracle. Neither establishes physical device
wiring, firmware authority, SMMU stream ownership or timing. Physical claims
require the exact board/device and allowed/denied operation evidence; DMA
requires additional actual transfers. Physical failure narrows claims rather
than being hidden by a software backend.

**Risk/stopping criterion.** Reject/reframe if the candidate reduces to field
comparison/schema checking, all cases are already caught by the baseline,
checker and oracle share a faulty derivation, only known defects reappear,
or no useful resource-bearing backend can be supported safely. A model result
alone cannot justify the cost of Linux/vGIC or establish research novelty.

## B - Fail-closed resource lifecycle under partial failure

**Problem and prior work.** Driver initialization, guest failure and resource
release can leave stale memory/IRQ authority or uncleared device state. Generic
revocation, device-assignment constraints and reset interfaces already exist
(P2,P5,P6). K1's attach ordering is a baseline bug, not the proposed research
contribution. A ring freed inside Linux is not physical-device revocation.

**Falsifiable question.** For one recoverable non-DMA device/backend with a
defined resource set, can a HAVEN-owned lifecycle protocol preserve exclusive
authority through partial startup and failed teardown, and safely complete
reuse without trusting guest cooperation, under a frozen failure schedule?

**Hypothesis and contribution.** A specified transition protocol and trusted
postconditions prevent stale old-owner access or simultaneous grants in every
tested transition, while at least one meaningful failure/recovery path permits
new useful I/O. “Always deny reuse” is a safe but functionally insufficient
baseline. Proposed contribution is the bounded transition property and
fault-injection evidence, not invention of revocation or object lifecycle.
Capability deletion alone cannot prove a hardware engine has stopped.

**Boundary and architecture.** HAVEN owns grant/revoke, mask/drain requirements,
translation invalidation, trusted reset confirmation and admission of the next
owner. KDAL/Linux is untrusted: stale work or forged readiness must not bypass
those conditions. A lifecycle control interface may be needed, subject to a
separate architecture review; ordinary data operations remain direct within
admitted resources. Do not place KDAL in EL2. No new interface is implemented
or authorized by presenting this candidate.

**Useful roles.** KDAL supplies actual attach/use/failure/unload behavior and
stale-work fixtures; HAVEN enforces transitions and denies unsafe reuse; ELMOS
repeats selected failures/timeouts and verifies state and resource ownership
before/after. Each component contributes an observable contract, not a name
in a launch diagram.

**Mechanisms and cost.** First model/interleaving study:2–4 days. A useful
enforced prototype is roughly10–25 days beyond the common guest baseline,
plus3–8 days for KDAL failure fixtures and ELMOS campaigns; real reset/device
integration adds hardware-dependent effort. Required missing mechanisms include
explicit lifecycle state, stale-authority tests, actual IRQ/MMIO revocation
and recovery evidence. Current whole-VM recreation does not prove live device
reuse and cannot stand in for that protocol.

**Alternatives and measurements.** Compare whole-VM recreation, fail-stop/no
reuse, cooperative-only cleanup, and the proposed trusted transition protocol.
Do not weaken the threat model to make cooperative cleanup count as secure.
Measure unsafe transitions/stale access, ownership overlaps, successful useful
reuse, leaks/reset count and observed recovery cost. Timeouts are bounded test
conditions, not worst-case real-time guarantees. Inject init failure after
each boundary, lost/false guest acknowledgments and stale interrupt/work after
revoke. Valid uninterrupted operation and isolated expected-denial cases are
controls.

**QEMU versus hardware.** A model can find a bad interleaving; QEMU can later
exercise CPU/IRQ ownership transitions. Physical reset, outstanding bus traffic
and device state need a real selected device. A future DMA variant needs actual
IOMMU mapping/revocation/drain evidence and is outside the first non-DMA scope.
No such hardware is currently available.

**Risk/stopping criterion.** Reject if safety requires trusting a guest's
quiescence assertion, if only whole-VM reset works, if no independently verified
reset enables useful reuse, or if the claim needs unavailable DMA containment.
The question must not be rescued by labelling a software ring a physical device.

## Comparison and provisional recommendation

| Criterion                               | A: demand/binding consistency                                                             | B: failure lifecycle                                                                      |
| --------------------------------------- | ----------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| Primary-source distinction              | Narrow fault-coverage/binding gap must be demonstrated; generic generation is established | Specific transition/reuse property needed; generic revocation/reset is established        |
| Cheapest rejection before Linux/vGIC    | Frozen host/model fault pilot,1–2 days                                                    | Transition/interleaving pilot,2–4 days; usefulness still depends strongly on device reset |
| New trusted runtime mechanism initially | None for rejection study; later existing admission/binding needs scoped checks            | New lifecycle/admission transitions likely                                                |
| Hardware dependency                     | Can reject early without hardware; physical claims later conditional                      | Useful physical-reuse claim strongly hardware dependent                                   |
| Current recommendation                  | **Provisional first investigation**                                                       | Reserve alternative if a concrete lifecycle/device opportunity appears                    |

Timing/interference is not a third candidate in this shortlist. Without a
selected hardware/workload/control mechanism and physical measurements, an
additional timing question would add uncertainty rather than a bounded initial
decision. No real-time hypothesis is smuggled into the software-ring milestone.

Proceed only to [A's rejection pilot](NEXT_EXPERIMENT.md) for the next research
decision. Passing it warrants further scoped feasibility work, not automatic
Linux/vGIC implementation, CP1 adoption or a novelty claim. If it fails,
reframe the question before investing in the common guest infrastructure.
