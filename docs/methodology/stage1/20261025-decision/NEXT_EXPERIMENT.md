# Proposed next experiment P-A0: reject a weak consistency question early

**Status: approved by the workspace owner 2026-10-31; steps 1-7 executed;
completed 2026-11-01 with verdict REJECT/REFRAME (candidate A as framed) and
stop-as-inconclusive on the device question; owner decides at G1.
See the [P-A0 evidence package](../20261031-pa0/README.md).** Owner: HAVEN research decision; ELMOS
evidence/orchestration and KDAL operation requirements are necessary inputs.
This is a1–2-person-day, host/model feasibility study, not CP1 implementation,
a Linux/vGIC feature or an isolation result. No new runtime ABI or page table
is needed. Stage0 closure and EP-A2 do not automatically approve this scope.

> **Note, 2026-10-31:** F3 is not an existing control, despite its row in the
> fixed test set below. The baseline accepted it, because
> `hv_stage2_map_partition` takes no authorised-owner input. See
> [P-A0 finding 3](../20261031-pa0/README.md). The protocol text is unchanged.

## Decision and independent inputs

Named uncertainty: does candidate A add a meaningful semantic/binding check
beyond existing static-policy and structural validation, or only rename known
configuration assurance? Reject it before investing in guest infrastructure
if no distinction survives a strong baseline.

Freeze before writing the candidate checker:

1. Trusted operator envelope E and expected results, authored independently
   of the candidate's resource emitter/checker. Freeze resource identity,
   IPA-to-target association, widths/permissions and confinement granularity,
   not just an address range. Record provenance and hash before cases/checker.
2. Actual KDAL software-ring demand: ordinary guest RAM only, no device MMIO,
   IRQ or DMA. It is a valid zero-external-resource control.
3. A **prospective**, finite read32/write32 register-operation fixture based
   on `kdal/src/backends/qemu/mmio.c` helper semantics. The helpers exist, but
   the ring lifecycle does not call them and they do not establish a working
   resource-bearing backend. Test addresses/IRQ identifiers are model data,
   not observed devices. Identify a credible safe non-DMA backend/device and
   authoritative map before treating any surviving case as a device-research
   opportunity; otherwise stop as inconclusive/reframe.
4. Frozen baseline: existing `test_core_stubs.c` memory/IRQ checks and relevant
   isolation/fault-injection checks, plus structural/range/version/hash checks.
   Do not remove existing protections to make the candidate win. Known ledger
   defects are non-scoring regression cases, not new contribution evidence.

All source identities come from current component revisions/diffs. Work in
disposable source copies/test fixtures under the existing evidence convention.
Use literal data and component APIs; do not create a stable cross-repository
manifest parser/generator/CLI. The prototype consumes KDAL requirements,
HAVEN policy results and ELMOS-style exact command/hash/result records. It
does not claim that the three runtimes were integrated or that ELMOS's missing
firmware mode was implemented.

## Small fixed test set

Use a model page P=`[0x10000,0x11000)`, owner1, CPU0, read/write32 at P+0x20,
no DMA. These are deliberately fictional host-test addresses. The trusted
oracle defines permitted operations separately from backend/guest/effective-
binding representations. The actual ring control requests none of P.

| Case | Input change | Independent expected outcome / purpose |
| --- | --- | --- |
| V1 | Actual ring demand, no MMIO/IRQ/DMA | Accept zero-external-resource control; inventing ring resources is failure |
| V2 | Consistent prospective P mapping/owner and read/write32 operation | Accept model operation; this is not actual hardware I/O |
| F1 | Base+size integer overflow | Reject; baseline structural/policy coverage control |
| F2 | Overlap with an already owned region | Reject; existing policy control, not a new research point |
| F3 | Wrong requested owner | Reject before resource grant; existing ownership control |
| F4 | Wrong artifact/version/hash | Reject; ordinary input-validation control |
| F5 | Guest-visible IPA and effective PA association disagree while each local range is valid | Detect semantic drift against E; model result only |
| F6 | Trusted read-only envelope, effective binding permits writes | Detect overgrant; identify whether baseline already checks it |
| F7 | Demand names current resource but an admitted/effective record refers to stale resource identity | Prospective/N/A and non-scoring unless an immutable build/config identity has source-backed semantics. If it reduces to F4's hash check, it is not a distinguishing semantic result; do not invent lifecycle epochs |
| F8 | Backend requires only a subrange; proposed page grant includes independently forbidden neighboring registers | Reject as not safely representable, or require a separately justified enforcement mechanism; silent rounding is failure |
| F9 | Non-ring IRQ requirement assigned to wrong target in a purely prospective resource fixture | Detect representation drift, or mark N/A if no credible IRQ-backed operation was selected; never attribute an IRQ to the ring |
| F10 | All representations agree on an unauthorized resource outside trusted E | Reject against E. Agreement among generated artifacts is not authorization |

F5–F10 are candidate distinctions to test, **not asserted baseline failures**.
If they are ordinary existing checks or cannot be grounded in a useful backend,
the study must say so. Do not count model-only IRQ/DMA behavior as delivered
interrupt/device support. Inspect any requirement already fully covered by the
primary verified-initialization alternatives before claiming a distinction.

## Execution and evidence

Compile only the necessary existing portable-policy harness and isolated
checker fixture. Per-build watchdog120s; per-case10s; two identical repetitions
to verify stable classification, not statistics. Timebox the study at two
person-days. Abort a failed preflight rather than building Linux or installing
new infrastructure to save the proposal.

For each baseline/candidate result record case ID, full input and hashes,
oracle verdict, observed return/side effect, detection checkpoint and failure
owner. Compare missed faults/false rejections by class, not aggregate pass
counts alone. Preserve any unsupported/N/A result. Require an independent
oracle/case review or a separately authored held-out mutation before scoring
a distinguishing result. If independence is not established, report the study
inconclusive on that distinction; no self-generated oracle can serve as proof.

## Continue, reject or stop

**Acceptance to continue scoped feasibility:** valid controls are accepted,
unauthorized cases rejected, classifications repeat, and at least one useful
non-structural case outside known defects escapes the strong baseline but is
detected by the candidate, with the independent-review/holdout condition above
satisfied. The case must have an independently justified
resource requirement and a credible later HAVEN enforcement probe that does
not first require Linux/vGIC. Recheck its distinction against P1–P6. This is
permission to propose that next bounded probe, not novelty or architecture
approval.

**Reject/reframe candidate A** if every effect is schema/range/hash checking,
the baseline covers all relevant cases, only known bugs are rediscovered,
all artifacts share the oracle's derivation, or the benefit depends on merely
invented device behavior. Any claim that a ring-only result proves device/DMA
isolation is itself a rejection of that interpretation.

**Stop as inconclusive** if no trustworthy resource map/operation/oracle or
necessary host harness is available within the timebox. Report the missing
input and revise the question; do not proceed into Linux/vGIC by default.

Host/model success can establish a viable distinction and fault taxonomy.
Actual enforcement still needs a later matched allowed/denied QEMU CPU/IRQ
experiment, and physical/DMA claims require their own hardware evidence.
