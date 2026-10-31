# Stage 0 component and boundary readiness

Date: 2026-10-25. Classification: AUDIT / DOCUMENTATION. This assessment uses
HAVEN `f775304ce9443dc9861ba1880e4208c7ae19b314`, ELMOS
`5ef5cd7fb974fca961e1af7f5a7f08ff47f70e95`, and KDAL
`1ab0dce6dedada4162325ba2101db63214ed896c`. Its experiments and
source provenance are in [README.md](README.md). "Source traced" does not mean
experimentally exercised. "Candidate" means no runnable cross-repository
interface was found. The independently reproduced working launch is the
preserved TF-A/HAVEN guest A run, not an ELMOS CLI or KDAL run.

## Observed repository responsibilities

| Repository | Actual producer / consumer behavior at pinned revision | Observed readiness |
| --- | --- | --- |
| ELMOS | `cmd/elmos/main.go` → `core/app/app.go` → `core/app/commands/qemu.go` → `core/domain/emulator/qemu.go` creates a Linux `-kernel` plus virtio disk/network/9p QEMU command, after requiring a mounted kernel image and disk. `core/infra/executor/shell.go::RunInDir` can invoke an explicitly supplied command and cwd. | Its existing `qemu run` is a Linux guest workflow; it has no HAVEN image, BL1/BL2/BL31, raw-loader address, PSCI conduit or bounded evidence-profile inputs. `elmos qemu --help` lists `run` and `debug`; the user doc's `-r` form is stale. Existing host `task --force build` and `elmos doctor` passed in the prior baseline, not repeated here. |
| HAVEN | CMake builds `haven.bin`, `guest_a.bin`, `guest_b.bin`; `boot.S::_start`, `haven_init`, `partitions_launch`, PSCI, stage-2, GIC and timer code produce the firmware handoff and guest behavior. | Manual TF-A launch for guest A and diagnostic CPU2 stage-2/IRQ/timer paths are observed. Default direct loader still faults at its EL2 HVC, and tracked secondary init/IRQ/timer paths remain incomplete. |
| KDAL | `src/core/module.c` initializes an out-of-tree Linux kernel module; `src/core/chardev.c` exposes `/dev/kdal`; driver ops call a backend; `qemubackend.c` performs in-kernel software ring-buffer I/O. | Its current QEMU workflow boots its own Linux kernel/rootfs. No KDAL module, ABI call, device image, boot dependency or runtime binding exists in HAVEN or its bare-metal guests. Source-only relationship within KDAL; no integration result. |

KDAL's `generic/devicetree.c` contains a real scan implementation, correcting
the earlier Stage 0 note that generic discovery was wholly absent. Its scan has
no call site in the current `kdal_core_init` path. Likewise, `virtioio.c`
contains transport helpers, but its setters and transfer routine have no
caller; the active QEMU backend still uses its software ring. The repository
can be assessed as an independent Linux module without requiring HAVEN.
The HAVEN demos are bare-metal EL1 stubs; `partition.c` calls A "Linux-class"
and B "RTOS", which does not establish that a Linux kernel or RTOS was booted.

## Producer/consumer boundary matrix

| Boundary; observed status | Required inputs / prerequisites | Output and success condition | First failure checkpoint / behavior | Owner and regression check |
| --- | --- | --- | --- | --- |
| **Host tools → HAVEN build**; exercised manually | Pinned HAVEN source and diagnostic diff if comparing historical logs; macOS tool versions; AArch64 compiler, CMake, Ninja; fresh isolated build path; `PLATFORM=qemu-virt` | Three raw images with recorded hashes and linker/load addresses; configure and build exit 0 | Cache source-path mismatch at HAVEN's old `build/`; use fresh path. `-mstrict-align` trial linked against undefined `memcpy`, exposing a HAVEN freestanding ABI gap. | HAVEN owns build/runtime ABI; ELMOS may later orchestrate. Regression: fresh CMake build, image hashes, link map, cache path and no unresolved compiler helper. |
| **TF-A/QEMU → HAVEN EL2**; manual replay observed | Matching BL1/BL2/BL31 from TF-A v2.15.0, BL33 `haven.bin` at `0x80000000`, A/B loader images at `0x80800000`/`0xa0800000`, secure QEMU virt/GICv3/SMMUv3, semihosting cwd | TF-A banner, HAVEN banner and A greeting/denial/resume marker in bounded UART log | If firmware banner absent: QEMU invocation/images/cwd first. If banner present but HAVEN absent: firmware handoff/image. If HAVEN reaches PSCI HVC fatal: HAVEN conduit/profile. | QEMU launch arguments/tooling belong to caller; firmware handoff to TF-A/QEMU; HAVEN owns its EL2 entry assumptions and PSCI choice. Regression: compare exact argv/image hashes and guest A markers against preserved independent replay. |
| **HAVEN → firmware PSCI CPU_ON → secondary**; temporary path observed | Declared SMC service for measured TF-A path; PSCI ID `0xC4000003`, target MPIDR, secondary physical entry, context ID; secondary stack/mailbox mapping; entry EL2 | CPU_ON success followed by independent CPU2 entry and readiness marker; nonzero status leaves B inactive | Direct loader HVC at EL2 fails before guests. TF-A temporary SMC returns zero but original success log does not prove readiness. Stack/mailbox Aff0 and inherited EL2 state are assumptions. | HAVEN owns conduit selection, argument/topology mapping, error and readiness state; firmware owns PSCI implementation. Regression: success, nonzero error, timeout/delayed ready and independent CPU2 marker under pinned firmware. |
| **HAVEN stage-2 → guests**; narrow QEMU paths observed | Both raw guests at fixed PA; table configuration, HCR/VTCR/VTTBR and per-CPU vectors before guest ERET | A allowed UART, denied B IPA; diagnostic CPU2 B denied A IPA in prior full-init experiment | Missing secondary EL2 init first failed at guest B entry; stage-2 denial only works after temporary setup. | HAVEN owns page tables, exceptions, guest launch. Regression: allowed and denied accesses from both CPUs with exact IPA/ELR/CPU logs; keep guest images and loader PA hashes pinned. |
| **HAVEN IRQ policy → GIC → guest virtual IRQ**; hardware delivery diagnosed, policy boundary unverified | CPU2 virtual GIC state and supported LR count; partition/IRQ ownership decision; guest EL1 VBAR/interface, interrupt handler | Test guest receives one INTID 60 and resumes. Production success additionally requires a permitted owner and denied cross-partition injection with no LR write. | `hv_el2_inject_exception(partition,...)` ignores partition ownership; separate route/ownership tables are not enforced at injection. LR scan assumes 16 while QEMU reports 4. | HAVEN. Regression: allowed/denied injection, correct CPU and vINTID, 4-LR limit and invalid/active state cleanup; negative `ICH_HCR.EN=0` control. |
| **HAVEN IRQ policy → physical GIC SPI**; synthetic delivery diagnosed | Assigned SPI, QEMU GICD route to CPU2, Group1 interface/mask state, handler that IAR/EOIs | SPI40 reaches CPU2 once, ownership-conflict API returns `HV_EPERM` without rerouting; diagnostic guest resumes | Before diagnostic setup, inherited `ICC_IGRPEN1=0` and no production handler. Current-EL2 IRQ vector is fatal. Software owner rejection and physical delivery are not yet one enforced path. | HAVEN GIC/IRQ policy. Regression: on-CPU delivery and EOI, denied owner, unchanged routing, inactive after EOI, current-EL2 IRQ path and unowned-device negative case. |
| **HAVEN timer policy → CPU2 CNTHP/PPI26**; register-level delivery diagnosed | CNTFRQ/CNTPCT, CPU2 PPI26 enable, physical CPU interface, compare/mask/rearm, handler/EOI | Diagnostic two events after 10ms future compares, no storm; masked and future controls produce zero | Existing `gic_v3_enable_irq(26)` enables CPU0, leaving CPU2 expired/unserviced. Policy table is global while CNTHP is per CPU. | HAVEN timer/GIC. Regression: CPU2 PPI readback and delivery, masked/future controls, independent CPU0/CPU2 deadlines, cancellation/rearm, no storm. |
| **ELMOS CLI → QEMU manual HAVEN recipe**; candidate, absent | Complete prior TF-A images, HAVEN images, exact QEMU arguments/cwd, 4s timeout and output location; no Linux disk/rootfs | Identical firmware and guest A markers plus machine-readable argv/hash/exit/timeout record | Current `qemu run` first checks mounted image, then kernel/disk, and its builder emits `-kernel`, disk, network, 9p, lacking TF-A and raw loader options. An immediate preflight error is ELMOS boundary ownership; a wrong command differs from the manual baseline. | ELMOS for launcher/capture only; HAVEN for image semantics. Regression: argv/cwd/paths byte-for-byte against pinned manual recipe, success marker comparison, missing-image and timeout cases; no change to HAVEN behavior. |
| **KDAL userspace → Linux module/backend**; source traced, no run here | Linux kernel/headers, `kdal.ko`, `/dev/kdal`, selected registered device/backend | Userspace read/write/ioctl reaches driver then QEMU in-kernel ring as coded | Missing module/device/backend belongs to KDAL/Linux guest workflow. The module requires a Linux guest absent from the current HAVEN demo. | KDAL. Regression within its own Linux QEMU workflow: module load, device enumeration, read/write and failure errno; independent of HAVEN until a research question requires mediation. |
| **HAVEN ↔ KDAL runtime**; **no implemented boundary** | A future research decision, Linux guest and defined HAVEN-mediated device ownership/interface would be required | No current output or success condition exists | No first failure to assign because the path is not implemented or invoked. Do not label this a broken integration. | No implementation owner selected. First decide whether KDAL is necessary for the thesis question; then define and review a HAVEN policy boundary before any code change. |

## Failure ownership rule

Run the independent manual launch with pinned images first. Compare firmware
banner, HAVEN entry, PSCI return, CPU2 entry, guest A/B markers, and final probe
result in that order. Locate the first missing or changed checkpoint. A failure
before the same checkpoint in both independent and orchestrated runs is not
attributable to ELMOS merely because ELMOS launched it. A run whose QEMU argv,
cwd or image hashes differ from the working manual launch points first to the
launch boundary. If those match and both runs fail after a HAVEN marker, inspect
HAVEN/firmware behavior. Evaluate KDAL only when its own Linux module path is
actually invoked. Do not modify another repository to hide an unexplained
checkpoint difference.

## Prioritized gaps and acceptance criteria

| Priority / owner | Gap | Acceptance evidence before adoption |
| --- | --- | --- |
| P0 HAVEN | Source/runtime alignment and compiler helper contract is sensitive to layout; two image variants faulted and a strict-alignment build lacked `memcpy` | Document EL2 memory attributes and permitted C alignment, enforce them at build, provide required freestanding helper ABI, then run multiple image layouts with no EL2 alignment fault. Preserve the failed builds as controls. |
| P0 HAVEN | Distinct direct-loader and TF-A PSCI contracts, full MPIDR/stack bounds, EL2 state and secondary ready signal | Direct path reports unsupported service cleanly; TF-A path uses its declared SMC and records CPU2 EL2 entry/readiness; error and delayed cases remain inactive. Do not globally change HVC to SMC. |
| P0 HAVEN | Full per-CPU initialization is incomplete and currently mixed with primary-only global init | Define initialization inputs, per-PE order, EL2 translation/cache handoff, ICC/ICH state, supported LR count, timer mask/ownership, and no shared-table reset on CPU2. Review code plus CPU2 guest/stage-2 regression. |
| P1 HAVEN | Physical IRQ dispatch and policy mediation are incomplete | Initialize physical interface, handle current-EL2 and lower-EL IRQs, IAR/EOI/deactivate safely, enforce common ownership on injection/routing, and prove allowed/denied cases. Preserve custom-handler probe as a diagnostic baseline. |
| P1 HAVEN | PPI26 enable targets CPU0 while timer runs on CPU2; timer policy has no CPU binding | Enable named CPU redistributor, show CPU2 readback/expiry/EOI plus masked/future controls and independent two-CPU deadlines. |
| P1 ELMOS | Existing QEMU CLI cannot express the known working TF-A/HAVEN launch | One scoped invocation accepts or points to pinned images, uses exact args/cwd, bounds runtime, captures UART/console/exit status and hashes, then reproduces A's known markers. Keep HAVEN unchanged for this experiment. |
| P2 KDAL | No HAVEN or bare-metal guest relationship exists | No implementation acceptance criterion yet. If a research decision needs it, first demonstrate a Linux guest and specify HAVEN-controlled MMIO/IRQ/DMA ownership; otherwise continue KDAL's independent Linux module checks. |
| P2 all physical boards | No hardware/boot logs | Acquire exact board/revision, firmware, recovery and UART evidence before any physical support criterion. |

## One bounded next integration experiment

Exercise **only ELMOS's host execution boundary** against the preserved working
TF-A/HAVEN images; no KDAL or tracked HAVEN change. In an isolated temporary
ELMOS driver, call its existing `ShellExecutor.RunInDir` with the exact QEMU
argv and cwd stored in `replay-a/run.json`, a four-second context deadline, and
capture the UART and process result. Treat the archived manual `replay-a` as the
independent baseline. Input files and hashes come from the preserved bundle.
Success requires identical image hashes and QEMU options/cwd, TF-A BL1/BL2/BL31
and HAVEN banners, guest A greeting, denied IPA `0x60000000`, post-fault marker,
recorded timeout/termination semantics, and no new HAVEN source modification.
A missing image or cwd/argument mismatch is ELMOS-owned. If manual and ELMOS
launches match yet both fail at the same HAVEN marker, the first failing HAVEN
or firmware checkpoint owns investigation. This is an experiment proposal, not
an assertion of an ELMOS-to-HAVEN integration already present.
