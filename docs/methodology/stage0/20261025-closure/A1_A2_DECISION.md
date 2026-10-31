# A1 execution-contract review and separate A2 recommendation

Date: 2026-10-25. Classification: AUDIT / RESEARCH / DOCUMENTATION. Owner:
HAVEN. Status: recommendations for concrete review; no flag, page table or
production hunk applied. Use the unchanged source revisions and diff inventory
in [repository-state.json](repository-state.json).

## Evidence reused and architectural explanation

The [original findings](../20261025-corrections/FINDINGS.md) and subsequent
[runtime campaign](../20261025-integration-design/RUNTIME.md) already supply
the relevant controlled comparisons. No new runtime experiment is needed to
choose the bounded next contract.

| Existing evidence | Observed result / consequence |
| --- | --- |
| Boundary archive `physical-spi/fault-disassembly.txt` and `uart.log` | 64-bit literal load at four-byte-aligned `0x80005d8c` faults, ESR `0x96000021`. This is a separate saved layout; it was not rerun in the later campaign. |
| Integration runtime archive `runtime/control-aligned/{gdb,uart}.log` | Exact old initialized-data failure reproduced: STP element alignment fails at FAR `0x80006fdc`, ELR `0x800038ac`, ESR `0x96000061`. |
| `runtime/aligned-abi-default/` | Same instruction/ESR still faults after the independent memcpy wrapper, now FAR `0x8000701c`. A2 does not fix A1. |
| `runtime/aligned-abi-strict/` and matched source patches | Same diagnostic source with strict compilation passes the previous checkpoint and diagnostic result. This is a mitigation comparison, not comprehensive initialization proof. |
| `runtime/target-abi-{default,strict}/` | Actual standard-name memcpy entry passes 4,160 offset/length cases at EL2 and A's denial/resume smoke path. |
| `register-summary.json` and full GDB logs | SCTLR_EL2 `0x30c50830` at primary entry → `0x30c51834` at post-init/fault. M=0 and A=0 at both; TTBR0_EL2/TCR_EL2/MAIR_EL2 remain0. HCR `0` → `0x80080039`; traditional non-VHE EL2 regime on Cortex-A72. |

Exact archive locations and identities are in the [evidence key](EXIT_CHECKLIST.md).
The runtime comparisons include temporary SMC and, in aligned variants, the
old diagnostic secondary/ISR setup. They are not a tested production UP profile.

Arm's memory-model guide describes stage-1-disabled data accesses as Device
and explains that unaligned Device accesses fault independently of SCTLR.A.
For LDP/STP, alignment is assessed per element, not the combined pair size.
These rules, the captured M=0 state and the actual offending addresses explain
the specific observed faults. A=0 was insufficient; enabling stage-2 for guests
does not create an EL2 stage-1 Normal-memory mapping.
[Arm memory model, §§3.2 and 12.1, printed pp10 and25](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Armv8-A%20memory%20model%20guide.pdf?revision=58b1dd0a-3800-4218-b21a-f95a0332034c)

This is a justified explanation for the captured non-VHE QEMU state, not a
diagnosis of every future alignment fault or every newer architecture feature.
It supersedes the older ledger's uncertainty about whether M was captured;
the old record is retained as history. General cache/publication correctness,
atomic helpers and Linux aliases remain unverified.

## Alternative comparison

| Concern | SC0: documented strict-access bootstrap | Explicit EL2 translation/cache initialization |
| --- | --- | --- |
| Supported scope proposed now | Pinned Cortex-A72/QEMU/TF-A primary bootstrap only, ending before secondary release, guest execution or external table publication/sharing | A separately designed EL2 runtime memory regime, initially identity-mapped for continuity |
| Mechanism | Compile all reachable pre-MMU C with strict alignment; audit assembly and helper accesses; enforce entry/profile limits | Establish valid EL2 tables, attributes, permissions, walk controls and table base; synchronize and enable translation with a reviewed cache/TLB transition |
| Match to current evidence | Existing strict/default comparisons support the access restriction; entry enforcement and whole-path coverage still need implementation review | No existing experiment validates an EL2 stage-1 table or transition |
| Smallest code scope | Compiler configuration plus entry checks/profile and access-coverage checks, separate from A2 and PSCI changes | New table layout/allocator or static tables, permissions/MMIO map, transition and cache maintenance; more failure modes |
| Limitations | Does not legalize invalid C pointers or unaligned inline asm, provide memory permissions, establish cache coherency, or certify atomics | Does not make arbitrary MMIO accesses safe; wrong attributes or missing mappings can break fetch, stack, vectors, devices or coherency |
| Linux implications | Safe bootstrap may precede Linux support, but does not approve Linux-time EL2 memory accesses or IRQ handlers | Can establish compatible Normal RAM and Device MMIO attributes for a later Linux runtime; requires its own proof/tests |
| Decision | **Recommend as the smallest bounded baseline contract to implement next after A2 review** | Defer implementation until runtime requirements justify it; keep as a likely extension, not an unrelated rewrite |

GCC15.2 documents `-mstrict-align` as controlling generated accesses that may
lack natural alignment. It is not an MMU/cache initialization operation and
does not cover handwritten assembly or make undefined C defined.
[GCC15.2 AArch64 options](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gcc/AArch64-Options.html)

An EL2 translation design must explicitly specify table/walk configuration,
memory attributes, mapping continuity and invalidation when interpretations
change. Current VTCR/VTTBR are a different regime.
[Arm memory management101811, §§5,7.2,8, printed pp12,20–21](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/LearnTheArchitecture-MemoryManagement-101811_0100_00_en.pdf?revision=1fdc3375-d81c-4457-b786-04fb98557de0)

These alternatives can be successive phases. Even a later implementation that
constructs Normal-memory tables in C needs safe pre-MMU code. Selecting SC0
does not forbid translation later; it avoids claiming that untested tables are
the minimum correction to a demonstrated bootstrap access mismatch.

## Proposed SC0 contract, assumptions and boundary

1. **Inputs:** exact pinned QEMU machine/CPU and firmware identities from the
   existing reference; AArch64 NS EL2h, non-VHE, little-endian entry with the
   documented M=0 handoff. Incoming cache/coherency state must match the chosen
   firmware profile. Reject an unsupported entry before interpreting indexed
   memory or relying on flat addresses; do not blindly disable a foreign MMU.
2. **Generated code:** the reviewed GCC15.2 target flags include strict
   alignment for every C translation unit reachable during this phase,
   including helpers. No LTO, target attribute, prebuilt object or build override
   may silently reintroduce a non-strict pre-MMU object. Preserve freestanding
   ABI checks, actual compile commands and disassembly.
3. **Manual accesses:** stack is16-byte aligned; BSS/mailbox/page-table and
   assembly access alignment is explicitly checked for each instruction's
   element size. MMIO uses its prescribed widths; generic memory-copy routines
   are not device-register accessors. Do not rely on source-variable alignment
   alone to justify a wider compiler-generated access.
4. **State meaning:** M=0 is an intentional bootstrap restriction, not RAM
   cacheability or permission isolation. Existing C/I bit writes are not proof
   that data accesses became Normal cacheable. Validate/respect the profile's
   entry and post-init state; a separate change is needed to alter cache policy.
5. **Phase end:** SC0 ends before CPU_ON/mailbox release, guest entry,
   concurrent mutation, externally walked table publication/enablement or cached
   guest-memory access not covered by an explicit memory/publication contract.
   In particular, current `haven_init` calls `smmu_v3_init` before global policy
   init; that driver programs SMMU_STRTAB_BASE and enables the controller. This
   step is outside SC0 unless its publication contract is separately admitted.
   A bootstrap-only profile must stop before it, and a future primary-only
   runtime profile must also omit the unconditional CPU2 launch. Existing
   SMC/secondary probes establish neither profile today. This boundary is not
   a new claim of a DMA defect: no device transfer was exercised.
6. **Excluded operations:** no atomic/exclusive helper, SMP safety, DMA-visible
   sharing or incompatible cache alias is admitted by this contract. Source
   `src/common/spinlock.c` contains LDAXR/STLXR/STLR; exact symbol search finds
   definitions but no call site in the inspected source. Strict flags cannot
   certify these unused helpers. Before such use, require an explicit supported
   memory type, ordering and sharing design.

Status: **recommended contract, not an implemented or supported tracked
profile**. Code-path enforcement/coverage remains engineering work; the
decision review itself is complete enough for preparation closure.

For Linux, the guest's own stage-1 MMU is independent of EL2's. Linux support
is not intrinsically forbidden by EL2 M=0, but guest Normal-memory aliases,
page-table publication, cache maintenance, exception paths and concurrency need
an explicit compatible contract before extending SC0. Arm warns against
incompatible attributes for aliases of the same physical location.
[Arm memory model, §13, printed p26](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Armv8-A%20memory%20model%20guide.pdf?revision=58b1dd0a-3800-4218-b21a-f95a0332034c)
No strict flag choice alone closes M1 Linux/vGIC or establishes timing.

## Regression matrix before claiming SC0 implementation support

| Check | Existing coverage | Required delta / pass and negative conditions |
| --- | --- | --- |
| Generated alignment | Matched default/strict failing layout and disassembly preserved | Inspect effective flags for all reachable bootstrap C/objects, including any source attributes; disallow escape through overrides/prebuilt objects |
| Known failure controls | Both older faults preserved; later campaign repeats initialized-data case | Reuse unchanged controls. Rerun literal-layout control only when validating an implementation not covered by the existing initialized-data case; name that missing coverage rather than repeat the full IRQ suite |
| Supported entry | Pinned firmware readbacks, M=0/A=0, primary/secondary snapshots | Implement primary precondition checks; unsupported EL/M/profile stops without interpreting a foreign translation regime |
| Phase/lifecycle bound | Current init publishes/enables SMMU tables and launcher releases CPU2 | Explicit bootstrap end checkpoint before SMMU/external walker setup; negative check proves no publication/enablement, CPU_ON, mailbox release or guest entry before extended initialization is admitted |
| Assembly/link layout | Faulting instructions identified; known stack/BSS conventions | Verify stack and linker-symbol alignment and representative data layouts; altered source/layout requires focused relevant regression |
| Compiler runtime | A2 default/strict build, symbols, recursion and target-copy checks pass | Retain checks for final compiler/config; missing or recursive helper is a build failure, not a firmware workaround |
| Fault behavior | Existing diagnostics preserve ESR/FAR/ELR | A deliberately invalid naturally unsupported access still yields the intended diagnostic; masking/skipping errors is not success |
| Linux/SMP/atomics/cache sharing | Not exercised by SC0 | Explicitly DEFERRED. Require separate runtime-memory review and tests before expansion; not a Stage0 closure requirement |

This matrix does not prescribe rerunning rows whose input and coverage are
unchanged. No new experiment was run for this decision.

## A2 assessed independently

**Recommendation: approve adoption of the existing narrow memcpy ABI patch,
subject to its concrete review gate and final application checks.** The
functional evidence is sufficient for that correction on the pinned compiler;
there is no remaining need for a generic Linux/vGIC experiment or a complete
A1 solution before reviewing A2.

Patch identity: [0001-arm64-export-memcpy.patch](../20261025-corrections/0001-arm64-export-memcpy.patch),
SHA-256 `d9c1b5189e7d3c15a89febf0ea30c56a4f8e7e394703544dbcf1e240b562dce2`.
One file, nine lines, ARM64-only forwarding entry; no compiler flag, startup,
IRQ or timer change. Evidence demonstrates the original missing libcall,
successful default/strict links, expected symbols, no recursive memory-helper
cycle, host guard and actual ARM64 semantics. The unchanged failure mechanism
in a shifted data layout remains A1, not evidence that memcpy semantics failed.

GCC's freestanding interface requires standard memory entries and exact
self-copy behavior for memcpy. The proposal supplies the demonstrated missing
entry only, not the entire runtime ABI.
[GCC15.2 freestanding requirements](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gcc/Standards.html)

Remaining acceptance conditions:

- Explicit approval of this exact ABI-only scope; no approval is inferred here.
- Verify clean applicability and preserve the unrelated local differences.
  On the actual adopted tree, perform the focused build/symbol/call-graph checks
  required by changed inputs. Reuse existing target execution evidence when
  compiler, relevant source and configuration are unchanged; repeat only a
  named uncovered input combination.
- Describe A1/default-runtime failure as an existing open baseline risk in
  the change record. No “boot fixed”, complete ABI or alignment-safety claim.
- Keep other memory entry exports, strict flags, page tables and probe code
  out of the patch. Pin the compiler/flags used for the accepted result.

The earlier broad implication that all A1 work must be finished before any
A2 adoption is superseded by this independent assessment. Its explicit review
gate remains. This task applies no production patch and declares no transition.
