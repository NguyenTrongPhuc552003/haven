# Scoped correction review and first patch proposal

Date: 2026-10-25. Owner: HAVEN. Status: design review completed for the scope
below; proposal prepared and validated in an isolated source export; adoption
awaits the existing explicit review gate. No proposal hunk has been applied to
the working implementation. This document does not approve a thesis architecture
or a transition out of Stage 0.

## Review outcome and correction order

The [ledger](FINDINGS.md) contains ten findings. Their dependencies lead to
the following sequence. Entries are separate review units, not one patch series
to adopt automatically. An investigation can end with a narrowed contract instead
of an implementation change.

| Order | Finding / proposed review unit | Dependency and exit criterion |
| --- | --- | --- |
| 1 | **A2: export the demonstrated missing `memcpy` ABI** | No PSCI/GIC/timer dependency. Build/symbol/semantics evidence below; first proposal only. |
| 2 | **A1: settle alignment and EL2 memory handoff** | A2 enables strict-build diagnostics. Capture effective state in the failing layouts and select a justified access/translation contract. A strict build alone is insufficient. |
| 3 | **B1: explicit QEMU firmware/conduit profiles** | Design can proceed alongside A1; runtime adoption must account for A1. Direct load fails cleanly for unavailable CPU_ON; the measured TF-A profile selects its SMC service. |
| 4 | **B3 then B2: bounded secondary entry and per-CPU preparation** | Use B1 and A1. Validate identity/EL/stack before indexed access, then initialize each PE and acknowledge only the readiness actually reached. Keep shared-table initialization primary-only. |
| 5a | **I1: implemented LR capacity** | Independent leaf after compiler gate; bound injection and pending-count loops. Saturation must stop at the implemented limit. |
| 5b | **T1: explicit private IRQ target API** | Independent leaf using B3's CPU mapping and the declared QEMU redistributor model. Validate enable and disable without changing another CPU. |
| 6 | **I2: physical CPU interface, vector dispatch and service** | A1, B2/B3 and GIC security/firmware ownership audit. Keep sources masked until service exists; handle supported current/lower EL entries and spurious IDs with per-CPU context. |
| 7 | **I3: owner/target/source-to-vINTID policy boundary** | B3, I1 and I2 for physical delivery. Deny unsupported remote injection and define revoke/pending/active behavior before enabling guest delivery. |
| 8 | **T2: CPU-bound CNTHP policy and expiry service** | B3, T1 and I2; I3 if delivering guest events. Independent two-CPU deadlines and negative controls precede any budget-enforcement claim. |

There is no dependency from these HAVEN corrections to KDAL. ELMOS launch
design is an independent host tooling concern. Neither repository should be
patched to compensate for HAVEN's boot, IRQ or timer failures.

The critical runtime chain is A1 → safe per-CPU preparation → physical IRQ
service → CPU-bound timer policy. The ABI export helps investigate the first
link; it is not a replacement for it. LR capacity and explicit PPI targeting
can receive focused reviews without waiting for a complete scheduling design.

## Isolation Guardian review record

HAVEN's `AGENTS.md` requests focused review for high-risk work. The read-only
Isolation Guardian reviewed the pinned source and preserved probes independently
of proposal preparation. The review found the ABI patch suitably independent
and recommended narrowing it to the actually missing `memcpy` symbol. We
incorporated its following corrections to the initial interpretation:

- The CPU2 PPI result exercises a diagnostic caller. Production does not
  currently enable PPI26 through that API; the API's boot-CPU behavior is
  explicitly commented. A local PPI capability and timer binding are required.
- Both LR loops require capacity bounds. The four-LR observation demonstrates
  the bad assumption, without demonstrating a fault on LR4.
- Public injection ignores the partition beyond validation and rejects
  vector>=32. vINTID60 delivered through the private architecture function
  does not validate the public API.
- Per-CPU preparation cannot reuse global exception-table reset. Shared
  exception context/counters also require a concurrency decision.
- Existing host tests named for secondary isolation explicitly exclude PSCI
  and assembly; policy error codes are not firmware-return regression evidence.
- Exporting every memory helper immediately would widen the semantics review.
  Existing `hv_memmove` pointer comparisons deserve a separate review.

These points are resolved in this proposal's scope and ledger. Broader B/I/T
designs have acceptance requirements, not implementation approval. The
Isolation Guardian reviewed the design; the final wrapper diff was checked
locally through the isolated validation described below.

## First proposed patch

Title: **arm64: provide the memcpy ABI for compiler-generated copies**

Base: HAVEN `f775304ce9443dc9861ba1880e4208c7ae19b314`.
Artifact: [0001-arm64-export-memcpy.patch](0001-arm64-export-memcpy.patch).
Implementation delta: one file, nine added lines in `src/common/string.c`.

The strict-alignment compiler creates a `memcpy` call while copying a stage-2
region structure. The runtime currently supplies `hv_memcpy`; its source macro
does not rename that generated call. The proposed standard-name entry forwards
to the existing helper only in ARM64 builds:

```c
#ifdef HAVEN_ARCH_ARM64
void *memcpy(void *dst, const void *src, size_t n)
{
        return hv_memcpy(dst, src, n);
}
#endif
```

The implementation already includes `stddef.h` and does not include the macro
header, so the definition retains the standard ABI name. A normal wrapper is
readable and does not depend on GCC alias attributes. The pinned GCC output is
a tail branch to the existing byte loop. Exact self-copy works as required by
the [GCC freestanding runtime contract](https://gcc.gnu.org/onlinedocs/gcc/Standards.html).
Partial overlap remains outside `memcpy`'s supported contract.

This proposal changes no production compiler option, PSCI instruction, startup
path, IRQ service, timer policy, public macro or repository interface. The
remaining standard memory ABI functions need separate semantics review. The
observed default build does not presently require them as unresolved symbols.

## Acceptance and validation

The validation runner exports pristine pinned source using `git archive`,
reproduces the strict build failure, applies only the proposal in that export,
then checks default and diagnostic builds. It does not include the session's
fatal-print or formatting changes and does not use the temporary IRQ probes.

| Criterion | Result and exact evidence member in `validation-evidence.tar.gz` |
| --- | --- |
| Prepatch known input fails on the compiler's missing ABI call | PASS expected failure; `build-baseline-strict.log`, `baseline-strict/CMakeFiles/haven.dir/src/core/mm/stage2.c.obj`; unresolved `memcpy` at `hv_stage2_map_partition+0x15c` |
| Proposal applies cleanly to the pinned input | PASS; `apply-check.log`, `apply.log`, `inputs.json` pins Git archive/proposal/fixture hashes |
| Default ARM64 build remains linkable without libc | PASS; `build-proposal-default.log`; unchanged normal target flags, `MinSizeRel` |
| Same strict build links after only the wrapper | PASS; `build-proposal-strict.log`; diagnostic `CMAKE_C_FLAGS=-mstrict-align` |
| Compatible ABI symbol and no unresolved runtime dependency | PASS; `symbols-proposal-{default,strict}.log`, `undefined-{object,image}-proposal-{default,strict}.log` |
| No recursive memory-helper cycle in inspected generated code | PASS for GCC15.2.0 and these flags; `disassembly-proposal-{default,strict}.log`. Wrapper jumps to `hv_memcpy`; helper uses byte accesses and does not call a standard memory entry. |
| Normal host object does not export the standard name | PASS; `symbols-host-guard.log`; only the ARM64 conditional test object exports it |
| Byte correctness, returned destination, zero length, exact self-copy, canaries | PASS native fixture; 4,160 combinations of offsets 0..7 and lengths 0..64, each also checked for self-copy; `run-host-fixture.log` exit0 and `commands.json`. This executes only `string.c` with the ARM64 define on the host. |
| All existing work and reference evidence preserved | Verified using captured diff and archive hashes; final repository status in `verification.json` |
| Runtime ARM64 safety under changed image layout | **Not run / unresolved A1.** The wrapper adds code and can move image data even if the default path does not call it. Build success cannot close this acceptance risk. |

Reproduction, with a fresh output directory:

```sh
python3 haven/docs/methodology/stage0/20261025-corrections/validate_proposal.py \
  --haven haven --work /private/tmp/haven-stage0-corrections-abi-replay
```

Exact per-command argv/cwd, return codes and 120-second bounds are preserved in
the archive's `commands.json`. All commands terminated normally; the pristine
link failure was the expected negative control. CMake4.4.4, Ninja1.13.2 and
GCC15.2.0 were used; see archived version logs for the full strings. Existing
RWX LOAD segment and discarded build-id warnings remain. No QEMU, whole host
suite, physical-board, PSCI, IRQ or timer regression was run in this review.

**Adoption gate:** ready for proposal review; build/ABI criteria pass and
ARM64 runtime acceptance remains pending. This is not merge/adoption readiness.
Approval can authorize the next disposable runtime validation. Before tracked
adoption, require a bounded ARM64 smoke check with the approved boot profile
and A1's layout-sensitive controls, and resolve or explicitly bound the memory
contract risk. Do not mix that test's harness or firmware/startup workarounds
into this patch. These checks cannot certify a complete freestanding ABI or a
production strict-alignment policy.

The user explicitly requested stopping with this proposal. The current
Stage 0 baseline also requires review before temporary boot/CPU-init code is
adopted. No tracked source adoption or cross-repository implementation is
authorized by completion of this document.
