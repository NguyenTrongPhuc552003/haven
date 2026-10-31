# Proposed component contracts and failure ownership

Status: review draft; none of the new interfaces below is implemented. All
three repositories are required. [ARCHITECTURE.md](ARCHITECTURE.md) identifies
observed paths and placement. This document specifies the proposed boundary
without authorizing a runtime ABI or coordinated code change.

## Common compatibility and lifecycle rules

`haven-guest-profile/1` and `elmos-run-manifest/1` below are proposed build-time
formats, not current stable ABIs. The first describes HAVEN-approved static
resources; the second records artifact identities and host execution. The
first implementation must reject unknown versions/fields and missing required
hashes. At present, profiles are reviewed static configuration compiled into
HAVEN; this design does not silently add a runtime JSON parser in EL2.

The profile is authoritative for the guest DT and memory map. ELMOS may
generate/stage files from it and report mismatch, but cannot enlarge admission.
Hash the effective profile, source revisions/dirty diffs, compiler/config,
Image/module/initramfs/DTB, firmware and workload. KDAL's reported 0.1.0 is not
sufficient compatibility evidence by itself; pin its UAPI bytes and exact
kernel build. AArch64 clients only initially. Guest-visible IPAs and loader
PAs are distinct fields; derive/check their relation, size, alignment and
overflow. Do not load a guest Image at the demo address without reading its
header and verifying all occupied ranges against the admission map.

Initial end-to-end bounds are test watchdogs, not real-time guarantees:
five minutes per build command, 60 seconds firmware→PID1, 15 seconds module
load, five seconds per short operation, ten seconds orderly guest teardown,
then three seconds SIGTERM and two seconds kill/reap fallback. Checkpoints
share a 120-second guest-runtime cap. These are proposed limits to pin in a
run manifest, adjustable only as recorded test configuration. Existing four-
second bare-metal references retain their own bounds. Missing readiness never
becomes success by waiting longer.

## C1 - ELMOS ↔ HAVEN build and admitted guest profile

| Field                            | Contract                                                                                                                                                                                                                                                                                        |
| -------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Producer / consumer / owner      | HAVEN produces source, build recipe and approved profile; ELMOS invokes/consumes them and stages output. HAVEN owns policy/config semantics, ELMOS owns orchestration. Build-time dependency only.                                                                                              |
| Inputs / outputs / compatibility | Pinned HAVEN source+approved diff, compiler, platform and profile version1 → image/ELF/map, profile hash, guest DT source and artifact requirements. Start with a distinct UP Linux profile; preserve demo profile for regression.                                                              |
| Trust / resources                | Host tooling may request a profile; trusted HAVEN code enforces its own compiled limits. CPU0 guest RAM/DT/image/console/vGIC/counter admission only; no physical backend or DMA resource. Reject overlap with EL2, reserved canary and firmware.                                               |
| Order / readiness / timeout      | Configure in a fresh source/build path → build → inspect images/header/ranges → verify hashes. Ready means complete compatible artifacts, not a boot. Five-minute command watchdog; no artifact promotion after nonzero/timeout.                                                                |
| Failure / teardown / recovery    | Preserve failed logs and partial output under unique run ID; do not overwrite known-good evidence or run cleanup in an unrelated tree. Rebuild from the pinned source in a fresh path.                                                                                                          |
| Tests / evidence / attribution   | Positive valid profile/default build; negatives unknown version, overlap, overflow, unsupported CPU/device and config mismatch. Save command/config/compiler/map/hash. Compiler/link/profile rejection inside HAVEN recipe belongs to HAVEN; wrong invocation/input selection belongs to ELMOS. |

## C2 - ELMOS ↔ Linux/KDAL artifact preparation

| Field                            | Contract                                                                                                                                                                                                                                                                                                                                                                   |
| -------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Producer / consumer / owner      | ELMOS produces a pinned Linux build and packaged initramfs; KDAL produces module and UAPI test artifacts using that build. ELMOS owns packaging; KDAL owns module/helper semantics. Build-time KDAL→Linux dependency; no KDAL→HAVEN library dependency.                                                                                                                    |
| Inputs / outputs / compatibility | Verified Linux6.6.80 source candidate or separately approved pinned version, UP config, toolchain/sysroot, KDAL revision+K1 correction → uncompressed Image with built-in initramfs, matching `kdal.ko`, AArch64 static test, module release/vermagic, config/Module.symvers/header hashes and bundle manifest. Kernel/module compiler compatibility is explicit.          |
| Trust / resources                | Only guest code/files are packaged. KDAL code does not acquire host devices, EL2 symbols or an implied right to physical I/O. Software rings consume ordinary guest RAM; no assigned IRQ/DMA. External source remains a dependency, not a fourth thesis implementation repository.                                                                                         |
| Order / readiness / timeout      | Build one external-initramfs-capable kernel/config and prepare matching module headers/symbols → build KDAL+fixture → construct deterministic cpio → produce final Image with built-in initramfs → recheck release/config/symbol compatibility. Ensure the initramfs inclusion step does not silently change module ABI. Pin archive order/owners/mtime and tool versions. |
| Failure / teardown / recovery    | Missing compiler/config/Image/module is preflight failure; no forced module load or stale artifact substitution. Keep component build logs; rebuild only the first mismatching artifact in a new output path. Five-minute watchdog per command; a full kernel build needing more must have a reviewed bound before execution.                                              |
| Tests / evidence / attribution   | Independent direct-QEMU Linux boot to PID1 first; module load, exact ring round trip, API errors, unload/reload. Negatives wrong vermagic/UAPI/hash rejected rather than forced. Save full kernel config, build commands and test results. Identical KDAL failure in independent Linux baseline belongs first to KDAL/kernel compatibility, not HAVEN.                     |

Linux kbuild needs the matching prepared kernel build; versioned module
symbols may require the full build, as described in the
[official external module guide](https://docs.kernel.org/kbuild/modules.html).
The final initramfs build procedure needs an explicit dependency check to avoid
the kernel/module/initramfs packaging cycle; ordinary source dependencies stay
one-way. An alternative reviewed fixed kernel plus separate initramfs payload
may be chosen if the two-pass built-in method changes ABI identity.

## C3 - ELMOS → QEMU/TF-A → HAVEN

| Field                            | Contract                                                                                                                                                                                                                                                                                                                              |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Producer / consumer / owner      | ELMOS generates typed QEMU launch from pinned artifacts; QEMU/TF-A performs firmware handoff; HAVEN consumes the EL2 entry. ELMOS owns process/files/capture; firmware owns firmware behavior; HAVEN owns entry validation. Runtime platform dependency.                                                                              |
| Inputs / outputs / compatibility | Structured firmware mode, image hashes/load PAs, named BL2/BL31 cwd files, machine/CPU/version/features, runtime cap → actual argv/cwd/host/tool metadata, serial/stdout/stderr, exit/signal/reap record and entry checkpoint. Reuse preserved TF-A2.15.0 bytes until rebuilt with recorded provenance.                               |
| Trust / resources                | Finite QEMU settings, no arbitrary executable/shell/hooks/raw extra arguments. Semihosting is explicit for the trusted reference inputs. No Linux volume/disk/9p/network prerequisite in firmware mode. HAVEN validates EL2 and its profile before admitting a guest.                                                                 |
| Order / readiness / timeout      | Validate/stage → spawn firmware → HAVEN entry → profile admission. Linux slice has a primary-only profile, not an unreviewed global HVC→SMC substitution. Use the runtime cap and graceful termination sequence above.                                                                                                                |
| Failure / teardown / recovery    | A firmware banner or CPU_ON success is not guest readiness. Missing entry stops at that checkpoint; terminate/reap process and preserve files. Recreate the VM for retries. No board flashing or live partition restart.                                                                                                              |
| Tests / evidence / attribution   | First reproduce the preserved bare-metal launch through the typed launcher, then the new Linux profile. Negatives missing/hash-mismatched image, wrong cwd, unsupported version/options and watchdog. Compare exact staged bytes/options against manual baseline; divergence before firmware entry belongs to launch/firmware inputs. |

The [prior minimum launch contract](../20261025-corrections/ELMOS_LAUNCH_CONTRACT.md)
supplies detailed generic host requirements; its historical KDAL omission is
superseded for the final system by the current scope and C2/C5/C6.

## C4 - HAVEN → Linux guest entry, interrupts and lifecycle

| Field                            | Contract                                                                                                                                                                                                                                                                                                                                                                                                    |
| -------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Producer / consumer / owner      | HAVEN produces admitted CPU/memory/device state; Linux consumes the standard ARM64 boot interface. HAVEN owns resource admission, primary guest lifecycle and virtual frontend. Runtime dependency; Linux is outside HAVEN's trusted core.                                                                                                                                                                  |
| Inputs / outputs / compatibility | Reviewed static profile1, actual Image header and guest DTB in admitted RAM, known entry IPA → Linux early entry, PID1 readiness with expected artifact ID. x0=guest DTB IPA; x1..x3=0. Match the selected kernel's boot protocol and relevant EL1/cache/counter/GIC state. No host DTB passthrough.                                                                                                        |
| Memory / IRQ / DMA               | Stage-2 includes only declared RAM and diagnostic console, excludes EL2/canary/other devices. Guest DT describes the same map. Bounded vGICv3 distributor/redistributor MMIO plus virtual ICC state and admitted console/timer events; supported accesses enumerated from the pinned Linux driver. CNTV/PPI27 service is required. CNTHP/PPI26 budget enforcement and DMA are not needed for software KDAL. |
| Order / readiness / timeout      | Validate/load → prepare primary state → stage-2 + virtual frontend → ERET → earlycon → PID1 within60s. Guest DAIF/state must follow the boot contract; mask physical sources until handlers/ownership are installed. No guest SMP or CPU_ON.                                                                                                                                                                |
| Failure / teardown / recovery    | Supported virtual-MMIO accesses are emulated; unknown/denied operations fail closed, never skipped generically. Define terminal rejection/halt for this test profile, with exact PC/IPA/source record. Startup failure/timeouts halt/recreate the VM. No claim of live pause/resume/reset.                                                                                                                  |
| Tests / evidence / attribution   | Independent Linux control, entry register/DT/header checks, timer ticks and PID1, vGIC capacity/EOI/allowed-source tests, denied MMIO/unknown access and excluded-page store. Save stage-2 maps, ELF/fault PCs, register snapshots and DT dump. If the same Image/config works directly but fails after matched HAVEN entry, inspect HAVEN state/frontend before editing KDAL/ELMOS.                        |

The standard entry protocol is documented by
[Linux ARM64 booting](https://docs.kernel.org/arch/arm64/booting.html).
This proposal must use the selected version's requirements; the current online
document also contains features absent from Cortex-A72 and is not a license to
program every listed optional register.

## C5 - Application → Linux `/dev/kdal`

| Field                            | Contract                                                                                                                                                                                                                                                                                                                                                                                                                         |
| -------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Producer / consumer / owner      | KDAL produces `/dev/kdal`; an AArch64 fixture consumes ioctl/read/write. Owner: KDAL. Runtime in-guest syscall boundary, not EL2 ABI.                                                                                                                                                                                                                                                                                            |
| Inputs / outputs / compatibility | Exact `include/kdal/ioctl.h` hash and GET_VERSION0.1.0, names selected through `kdal_ioctl_devname` (32 bytes), fixed request structures → list/info, exact round-trip bytes and return counts/errno. No32bit compatibility claim.                                                                                                                                                                                               |
| Trust / resources                | Linux validates user buffers; KDAL validates fd selection and backend state. Mode0666 is not partition admission. Ordinary KDAL calls remain inside guest resources; HAVEN does not authorize each ioctl. No direct IRQ/DMA in the ring operation.                                                                                                                                                                               |
| Order / readiness / timeout      | PID1 → successful insmod after K1 fix → version/list/select → round trip. Require module-ready checkpoint, not `/dev/kdal` creation alone. Fifteen seconds load/five seconds operation watchdogs. Single client; no concurrent hotplug.                                                                                                                                                                                          |
| Failure / teardown / recovery    | Invalid name returns ENODEV and preserves prior selection; write on an unselected fd returns ENODEV. Kernel faults or unexpected success fail the test. Close all fds, unload/reload in the independent baseline; integrated VM reset handles terminal faults.                                                                                                                                                                   |
| Tests / evidence / attribution   | Write/read a nonce-bearing 32-byte payload and compare exact bytes/counts, separate fd selection; invalid name, bad user pointer EFAULT with valid selected powered-on device and nonzero count, power-off I/O EIO then restore. Save module/UAPI hashes, request structures, numeric errno and payload digest. API errors prove KDAL validation, not hypervisor isolation. Same failure directly and under HAVEN is KDAL-first. |

## C6 - KDAL driver → backend inside HAVEN's resource envelope

| Field                            | Contract                                                                                                                                                                                                                                                                                                                                                        |
| -------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Producer / consumer / owner      | KDAL driver consumes `kdal_backend_ops`; software backend produces ring reads/writes. KDAL owns this internal API. HAVEN independently enforces the outer guest envelope. Runtime KDAL→Linux only; no backend→EL2 callback.                                                                                                                                     |
| Inputs / outputs / compatibility | Pinned `backend.h`/core API and enabled QEMU/example object configs, live driver association, bounded count≤4096 → bytes accepted/read or kernel errno. First payload is32 bytes, avoiding saturation assumptions.                                                                                                                                              |
| Trust / resources                | Linux heap contains ring/buffers. No physical register, host device, interrupt or DMA assignment. A later physical backend needs explicit admitted MMIO/IRQ and DMA containment before use; the module's presence grants none.                                                                                                                                  |
| Order / readiness / timeout      | Core lifecycle: chardev/debugfs → backend register/init/enumerate → driver/device registration/attach/probe. K1 must be fixed before this path can be accepted. Require successful entire module init, then test bounded I/O.                                                                                                                                   |
| Failure / teardown / recovery    | Probe association rollback and ring/config cleanup on failure; reverse driver/backend teardown after fds close. Do not call unsafe concurrent removal supported. Module init failure does not cause HAVEN to enlarge resources or bypass isolation.                                                                                                             |
| Tests / evidence / attribution   | Independent attach success/failure, loopback/canaries, unload/reload and failure-unwind controls; no stale ring on reload. Preserve kernel logs and first fault trace. A KDAL allocation/probe failure belongs to KDAL; a host stage-2 fault on a resource not admitted belongs to guest misuse/profile compatibility investigation, never automatic admission. |

## C7 - Negative workload → HAVEN denial → ELMOS evidence

| Field                            | Contract                                                                                                                                                                                                                                                                                                                                                             |
| -------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Producer / consumer / owner      | A HAVEN-owned Linux test module issues one controlled guest-EL1 store; HAVEN produces terminal denial state; ELMOS captures external debugger/memory evidence. HAVEN owns the fault fixture and policy; ELMOS owns capture. No production ioctl/hypercall is added to KDAL.                                                                                          |
| Inputs / outputs / compatibility | Known probe ELF/PC and chosen guest IPA; reserved scratch HPA page containing canary, excluded from all guest maps; image/profile hashes and test nonce → exact source CPU/ELR/IPA/ESR, terminal checkpoint and unchanged physical canary. Debugger uses physical-memory mode for oracle reads.                                                                      |
| Trust / resources                | Guest logs can be forged. The oracle stops/observes EL2 and reads reserved physical memory externally. Choose test RAM separate from EL2/firmware/code; never target controller registers or actual devices. Stage-2 admission is authority; DT omission alone is insufficient.                                                                                      |
| Order / readiness / timeout      | Run after KDAL's successful operation/API negatives and fd closure. At the controlled abort, verify exact identity and no unauthorized store; leave offender terminal. Five seconds probe cap; host stop/reap then recreate VM.                                                                                                                                      |
| Failure / teardown / recovery    | No fault, wrong PC/IPA, mutated canary, generic skipped instruction or missing oracle is failure/inconclusive, not a pass. Do not report guest crash alone as isolation success. Keep a separate deliberately admitted scratch-page control that stops at a known post-store checkpoint and must change the physical canary to prove the probe/oracle works.         |
| Tests / evidence / attribution   | Matched allowed/denied scratch-page configurations differing only in that admission; exact hashes, source diff, raw logs, debugger commands/readbacks and termination record. Wrong profile or unexecuted probe belongs first to configuration/test tooling; an unauthorized store completing through stage-2 belongs to HAVEN. No DMA or physical-device inference. |

## First-failure rule

Compare: artifact preflight → QEMU/firmware → HAVEN entry/admission → Linux
early entry → timer/vGIC → PID1 → module init → version/selection → bytes →
denial oracle → process reap. Use the independent baseline at the same pinned
component inputs. Assign the earliest divergent checkpoint before proposing
code. Patch the owning component only; no cross-repository workaround for an
unexplained failure. Teardown failures are recorded independently of workload
success. No automated fallback may disable stage-2, expose the physical GIC,
force-load mismatched modules or switch to a direct Linux boot and call it
integration.
