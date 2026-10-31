# Isolated ARM64 ABI/alignment validation

Date: 2026-10-25. Classification: EXPERIMENT / AUDIT. Owner: HAVEN.
Purpose: advance A2's pending target execution check and narrow A1's memory
state uncertainty using available preserved inputs. This is not an integrated
Linux/KDAL run or approval to adopt temporary source changes.

## Inputs, isolation and commands

Source base: `f775304ce9443dc9861ba1880e4208c7ae19b314`, exported with
`git archive`. Firmware: preserved TF-A2.15.0 BL1/BL2/BL31 from
`experiment-evidence.tar.gz::replay-a/`, with unchanged recorded hashes. The
exact failed aligned image and patch come from the archive's
`physical-spi-aligned/` members. The only candidate production hunk is the
unchanged [memcpy proposal](../20261025-corrections/0001-arm64-export-memcpy.patch),
SHA-256 `d9c1b5189e7d3c15a89febf0ea30c56a4f8e7e394703544dbcf1e240b562dce2`.
It remains unapplied to the workspace implementation.

The [runtime runner](runtime_validation.py) creates separate disposable source
exports/builds and records each exact patch against the pinned base. Aligned
variants replay the earlier complete diagnostic probe patch. Target-copy
variants add only the saved fatal diagnostic, temporary SMC transport and an
EL2 call to the earlier memcpy semantics fixture; they do not adopt a secondary
initializer. The fixture calls the actual standard-name entry without the
source alias and tests 4,160 offset/length combinations, return pointers,
self-copy, zero length and canaries. Disassembly of the fixture and helper was
saved. The probe setup, SMC and compiler option remain experimental scaffolding.

Executed from the workspace:

```sh
python3 haven/docs/methodology/stage0/20261025-integration-design/runtime_validation.py \
  --haven haven --work /private/tmp/haven-stage0-integration-runtime-approved
```

QEMU's machine/CPU/firmware/loaders match the preserved secure virt reference.
Each image uses a local Unix GDB socket and starts paused. Hardware breakpoints
observe primary `_start`, then `partitions_launch` or `hv_fatal_exception`.
The successful aligned probe also observes CPU2 at `stage0_prepare` before its
CPU initializer. GDB reads state without injected instructions or register
writes, then detaches. [QEMU's debugger documentation](https://www.qemu.org/docs/master/system/gdb.html)
describes this local socket and system-state observation facility.

Each configure/build command has a120s bound; GDB has12s, socket readiness1s,
post-detach execution4s, and SIGTERM has a2s kill/reap fallback. Thus the debug
run is bounded by those phases, rather than claiming the old four-second
wall-clock limit while paused. All five actual runs took about4.1s and used
SIGTERM after the post-detach deadline; no kill fallback was needed. A watchdog
or exit0 alone is not a pass. Exact argv/cwd, versions, phase bounds, exit codes,
ELFs/raw images/hashes, full register logs, build output and source patches are
preserved. No performance or latency statistic is inferred.

The first sandbox attempt failed to bind the Unix socket with `Operation not
permitted`; no guest ran in that attempt. The rerun was approved outside the
sandbox for the local socket. That failure and its command/log are preserved
as tooling evidence, not a HAVEN failure. No installation or network download
was needed.

## Results

| Run | Expected condition | Observed result / exact archive member |
| --- | --- | --- |
| `control-aligned` | Replay the exact previous primary alignment failure | Expected failure reproduced: ESR `0x96000061`, FAR `0x80006fdc`, ELR `0x800038ac`. `runtime/control-aligned/{gdb,uart}.log` |
| `aligned-abi-default` | Adding only memcpy does not certify alignment safety | Expected alignment failure reproduced: same ESR and instruction ELR; shifted FAR `0x8000701c`. `runtime/aligned-abi-default/{gdb,uart}.log` and source patch |
| `aligned-abi-strict` | Matched diagnostic source plus ABI and strict accesses passes former failing checkpoint | Reaches `partitions_launch`, A post-fault marker and diagnostic mode2 SPI result PASS with active readback0. `runtime/aligned-abi-strict/{gdb,uart}.log`. This uses the old custom service and does not verify production IRQ policy. |
| `target-abi-default` | Execute standard memcpy ABI at EL2 with existing target flags | `ABI TARGET PASS rc=0`, A denied load and post-fault marker, no EL2 fatal in window. `runtime/target-abi-default/uart.log` |
| `target-abi-strict` | Same target byte/canary fixture with diagnostic strict flags | Same byte-test and A markers pass. `runtime/target-abi-strict/uart.log` |

All five verdicts matched their stated conditions; the first two are
reproduced failure controls. The new aligned default and strict sources match
apart from build option; their image layout/instruction stream can differ.
This is evidence for strict accesses mitigating this diagnostic layout, not
proof of a general compiler/memory contract.

## Newly observed state

| Checkpoint | SCTLR_EL2 | HCR_EL2 | TTBR0_EL2 / TCR_EL2 / MAIR_EL2 |
| --- | --- | --- | --- |
| Primary TF-A entry, all runs | `0x30c50830` | `0` | all0 |
| Primary post-init or aligned fault, all runs | `0x30c51834` | `0x80080039` | all0 |
| CPU2 before diagnostic CPU init, strict aligned run | `0x30c50830` | `0` | all0 |

`SCTLR_EL2.M=0` throughout these checkpoints; A=0 both before and after, while
C/I become set by HAVEN. VTCR changes from0 to `0x80023559` on primary, but
that stage-2 control is not an EL2 stage-1 memory-map initialization. Full
logs and [register-summary.json](register-summary.json) retain the observations.
The faulting access is confirmed unaligned by the archived instruction and
FAR. We now know that HAVEN executes that access with EL2 stage-1 translation
disabled despite clearing A. A documented permitted-access/memory-attribute
contract is still required; these readbacks do not independently prove every
effective attribute or a correct cache/translation transition.

## Adoption status and limits

A2 now has **on-target semantic and narrow smoke evidence** in addition to
the earlier isolated build/link checks. A1 remains open: the default image
still fails under a meaningful initialized-data layout. Source review may
accept the memcpy ABI correction independently only with that unresolved
runtime baseline stated; this task does not apply it or open the adoption gate.
Do not combine strict flags, SMC, full-init or custom ISR with its patch.

The next A1 decision is a scoped strict-access execution contract versus an
explicit EL2 Normal-memory translation/cache setup, justified by architecture
and the supported toolchain. Neither is adopted here. Other layout/compiler
combinations, the original separate literal-load failure image, Linux guest
boot, secondary readiness, guest virtual frontend, KDAL and physical boards
were not validated in this campaign. GDB pausing precludes timing conclusions.

## Durable preservation

All 1,360 experiment files were copied to ignored
`haven/build/evidence/stage0-20261025-integration/` and frozen outside ignored
output in [runtime-evidence.tar.gz](runtime-evidence.tar.gz), SHA-256
`8321d1560da2cb8c7a2919f1219676d89d80daa80963afdd1689e532bde757da`.
[runtime-inventory.json](runtime-inventory.json) records individual hashes;
every archived member was read back and verified. Temporary sockets were
excluded; commands, source, logs, images, caches and disassembly remain.
`runtime/commands.json`, `runtime/inspection-commands.json`, run JSONs and
`execution.json` are authoritative for exact commands and termination.
The source export and recorded patches allow recovery without the temporary
build directory. Embedded build timestamps can change hashes on rebuild.
The archive is Git-visible but has not been committed or remotely backed up.
