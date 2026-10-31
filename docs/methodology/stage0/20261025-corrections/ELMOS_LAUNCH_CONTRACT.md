# Minimum ELMOS launch contract for the reference scenario

Date: 2026-10-25. Status: proposed contract for review, no ELMOS extension.
ELMOS revision `5ef5cd7fb974fca961e1af7f5a7f08ff47f70e95` is clean at capture.
The [preserved boundary assessment](../20261025-boundaries/BOUNDARIES.md)
traces its current Linux QEMU workflow. This document supersedes the older
proposal to expose a generic execution adapter as the next integration design.
An existing internal executor is an implementation detail, not a public launch
contract.

## Generic launch responsibilities owned by ELMOS

A finite, structured firmware-launch description should express these fields.
This is a requirements model; it does not select a CLI syntax, configuration
format, new dependency or general process-execution API.

| Requirement | Minimum behavior and failure condition |
| --- | --- |
| QEMU identity and machine | Architecture selects the QEMU executable through ELMOS's tool discovery; record resolved path/version and explicit machine/CPU/features, RAM and CPU count. Unsupported fields or incompatible machine/firmware mode fail before spawning. No arbitrary executable, shell string, pre/post hooks, or raw extra-arguments field. |
| Boot mode and firmware | Select firmware boot explicitly, with one BIOS image path and expected SHA-256. Do not require Linux `Image`, rootfs/disk, mounted ELMOS volume or module sync for this mode. Existing Linux launch retains its own prerequisites. |
| Raw payloads | Typed list of image path/hash, physical load address and raw-loading choice; PC-setting is explicit where supported. Validate files, integer overflow and payload overlaps before launch. The scenario supplies permitted addresses; ELMOS does not derive HAVEN isolation policy or rewrite image contents. |
| Firmware working files | Typed input-file mappings into a fresh run directory, including the exact filenames firmware opens. Hash/resolve input paths, copy or link without changing originals, record the chosen method and prevent filename collisions. Relative path/cwd semantics must match the reference. |
| Semihosting | Explicit supported QEMU option with recorded target, enabled only by the selected firmware profile. The reference uses native semihosting to load named firmware files. Treat these firmware images as the pinned inputs; an arbitrary file hook is unnecessary. |
| Console capture | File-backed serial UART plus stdout/stderr capture, explicit monitor/display settings, unique output directory. Preserve complete logs and capture/spawn errors. A byte-identical argv is desirable when retaining relative staged names; absolute run-directory changes must be recorded. |
| Bounded execution | Runtime deadline, graceful termination signal, kill fallback and final reap. Record deadline/exit/signal separately. For this reference: four seconds, SIGTERM, two-second fallback. A context that immediately kills QEMU is not equivalent to that policy. |
| Reproduction manifest | Record normalized launch description, actual argv/cwd, environment relevant to resolution, executable/tool version, input hashes/provenance, outputs, timestamps, return status, timeout/signal and cleanup result. Never replace process outcome with a marker verdict. |
| Checkpoint evaluation | Scenario-owned literal checkpoints and forbidden fatal markers, evaluated independently of exit0/timeout. Generic tooling collects and reports; HAVEN owns meanings and expected order. A missing input, changed hash, spawn error, absent checkpoint or lingering process is failure with its first checkpoint identified. |

`core/domain/emulator/qemu.go::Run` currently checks kernel and disk and then
calls the Linux argument builder. Its command includes `-kernel`, rootfs,
network and 9p. The CLI's `run` uses the mounted-workspace guard. None of that
expresses this firmware boot. `ShellExecutor.RunInDir` can select cwd and
`exec.CommandContext` accepts typed argv internally, but its default cancellation
and existing runner's signal handling do not establish the required graceful
timeout/capture lifecycle. No shell escape hatch is required or proposed.

## HAVEN-specific scenario inputs owned by HAVEN

ELMOS consumes these pinned values; HAVEN owns their compatibility, loader
addresses, guest behavior and firmware assumptions. TF-A owns its firmware.

| Field | Reference value / authority |
| --- | --- |
| Scenario and input manifest | `replay-a/run.json` in [experiment archive](../20261025-boundaries/experiment-evidence.tar.gz); directly indexed in [run-summary.json](../20261025-boundaries/run-summary.json). Use its image hashes, not newly rebuilt HAVEN as an implicit substitute. |
| Evaluator | Recorded QEMU11.1.2, `virt,secure=on,virtualization=on,gic-version=3,iommu=smmuv3`, `cortex-a72`, four CPUs, 2GiB. The alias/version combination is part of the reference; no other QEMU version is certified. |
| Firmware | TF-A v2.15.0 attributed to `da738d5eae93af342fdc4995dd3c05acb4c9d757`, preloaded BL33 at `0x80000000`. BIOS=`bl1.bin`; `bl2.bin` and `bl31.bin` staged under those exact cwd names; native semihosting enabled. |
| Payloads | `haven.bin` raw at `0x80000000`, A raw at `0x80800000`, B raw at `0xa0800000`. No loader sets CPU0 PC; BIOS/TF-A starts first and hands off to HAVEN. |
| Captures | Display none, monitor none, serial file `uart.log`; separate process console capture. Process stops on the bounded host deadline, rather than an invented guest exit command. |
| Known result | TF-A/HAVEN banners, A EL1 greeting, denied IPA `0x60000000`, A `Post-fault check OK`, and no EL2 fatal marker. The replay uses a historical temporary SMC image; B readiness is not a criterion satisfied by that image. |

Firmware SHA-256: BL1 `36e92729ffd0d3e7499dd0648d273f3cfa0bab51b07b8cce52c56465870c4ec7`,
BL2 `92b70dc998bf2b8cc2290c523b2d31b84926ae23ae554ce8c8252d0661046d1d`,
BL31 `588a7e29bad277db457721cbf91f87358356418c0717d7178844f64c38e92076`.
Replay HAVEN `da49b8ae191202c03fa94ddb664843e83fd8985999d9f5172d816fbd1f4526d8`,
A `00d4c4dd4cb94e6bba6b89c9c0b0bc4c962c45bcd39eba5b268ae722b9b2eeea`,
B `616671c42e10a6571169f6c96a15cb9581dade662909955c13f0945185c6fafb`.

Exact reference argv is preserved in that manifest and the prior README.
[TF-A's versioned QEMU guide](https://trustedfirmware-a.readthedocs.io/en/v2.15.0/plat/qemu.html)
explains firmware/cwd semihosting behavior;
[QEMU's generic loader](https://www.qemu.org/docs/master/system/generic-loader.html)
defines raw memory loading and the separate PC-setting option. Neither document
establishes the observed HAVEN checkpoint sequence; the reference log does.

## Acceptance for a later bounded ELMOS implementation

First review the structured launch design and obtain the workspace's required
approval for any coordinated interface change. Then one ELMOS-owned extension
could reproduce the archived bytes with HAVEN unchanged. Compare against a
fresh independent manual replay before attributing a discrepancy to ELMOS.

Required regressions: structured description produces the reference options
and cwd/file names; no Linux volume/kernel/disk prerequisite in firmware mode;
missing/corrupt/overlapping inputs and unsupported options fail before spawn;
paths with spaces remain single argv elements; deadline/interrupt captures and
reaps QEMU; all specified markers and fatal exclusions are evaluated. Existing
Linux launch command construction needs a focused regression for any shared
code changed. No ELMOS QEMU run was performed in this design review.

Ownership follows the first divergence: wrong arguments/cwd/staging/capture
belongs to ELMOS; identical images/options reaching a HAVEN boot/PSCI fault
belongs first to HAVEN/firmware investigation. Do not modify HAVEN to make an
orchestrator mismatch disappear.

## KDAL boundary

Integration is **undefined**. KDAL currently supplies a Linux kernel module,
`/dev/kdal` interface and independent backend workflow. The reference HAVEN
guests are bare-metal demos. No module, ABI call or runtime dependency joins
them. KDAL is neither an input nor an acceptance prerequisite for this scenario.
A future research decision must justify its role and define a HAVEN-controlled
device boundary before implementation. No KDAL extension is proposed here.
