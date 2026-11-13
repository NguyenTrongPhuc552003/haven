# Required artifact and proposed smallest vertical slice

Status: proposed design for review, 2026-10-25. Owner: HAVEN for architecture
and trusted admission; ELMOS for host reproducibility; KDAL for device
abstraction. All three components are required by the user's current scope
decision. Their exact runtime interfaces, research question and novelty remain
unapproved. Revisions and local differences are pinned in
[repository-state.json](repository-state.json).

## Observed system

| Component | Actual interface and observed limit |
| --- | --- |
| ELMOS | Existing CLI builds Linux/module/application artifacts and boots a Linux `-kernel`/disk/network/9p workflow. The prior manual TF-A/HAVEN launch is not expressed by its QEMU runner. Existing host build/doctor evidence is reused. |
| HAVEN | Builds bare-metal A/B images, maps fixed stage-2 ranges, ERETs with x0=0, attempts CPU2 CPU_ON. Preserved diagnostic images run guest A, CPU2 stage-2, IRQ and CNTHP probes. No tracked Linux handoff or Linux boot is established. |
| KDAL | Linux module entry → `kdal_core_init` → chardev/debugfs/backend/drivers. `/dev/kdal` read/write/ioctl forwards through a selected device. The QEMU backend owns software rings in Linux kernel memory. No current HAVEN import, guest image or runtime call links them. |

The evidence for those paths is in the [boundary assessment](../20261025-boundaries/BOUNDARIES.md).
Required final integration is a scope decision; it does not convert these
independent implementations into an observed integrated system.

## Placement comparison and selection

| Placement | Benefit | Added prerequisites / conclusion |
| --- | --- | --- |
| **KDAL in one Linux EL1 guest** | Preserves its module and UAPI, gives a usable abstraction, keeps Linux/KDAL outside trusted EL2 | **Selected proposed slice.** Requires Linux boot, constrained virtual interrupt frontend, guest timer, matched module environment and KDAL lifecycle correction. |
| Management Linux with KDAL serving other guests | Could centralize a future physical device service | Adds inter-guest transport, buffer admission, event notification, lifecycle/revocation and a service trust decision. Defer. |
| KDAL in EL2 | Superficial direct dependency | Requires replacing Linux facilities and enlarges HAVEN's trusted core. Rejected for this slice. |
| KDAL's independent Linux QEMU workflow | Cheapest independent functional control | Required comparison baseline, but cannot satisfy the final HAVEN-integrated artifact alone. |

The selected vertical slice uses one Linux guest vCPU on physical CPU0. QEMU
can retain the pinned four-CPU firmware model while only CPU0 is admitted to
this guest; other CPUs remain held by firmware. A reviewed single-partition
HAVEN profile must explicitly omit the current unconditional CPU2 launch.
Linux is configured UP and its guest DT lists only the admitted CPU. This
avoids requiring SMP, secondary readiness or guest CPU_ON for the first slice.

The first KDAL operation is a short byte-stream round trip through `uart0` on
its existing `qemu-virt` software backend. This exercises the abstraction's
version/enumeration/selection/driver/backend contract, including fd-local
selection and lifecycle. Its ring is guest RAM, with no device DMA or hardware
interrupt. It is a useful interface baseline; physical device access remains
an explicit later milestone.

## Proposed dependency map

```mermaid
flowchart TD
    E[ELMOS host preparation, builds and evidence] --> HIMG[HAVEN image and approved static profile]
    E --> GIMG[Pinned Linux, initramfs, KDAL module and test payload]
    HIMG --> Q[QEMU and pinned TF-A]
    GIMG --> Q
    Q --> H[HAVEN at EL2: trusted resource admission]
    H --> L[Admitted Linux guest at EL1]
    L --> U[Known userspace fixture]
    U --> K[/dev/kdal and KDAL module in Linux]
    K --> R[Software ring in admitted guest RAM]
    H --> N[Stage-2 denies excluded test page]
    Q --> C[ELMOS captures console, debugger oracle and manifests]
```

Build-time dependencies: ELMOS invokes component-owned build recipes; KDAL
builds against the pinned Linux build; HAVEN's guest profile defines admitted
resources and guest handoff. Runtime dependencies: Linux runs under HAVEN;
KDAL depends on Linux; the guest application depends on `/dev/kdal`. HAVEN has
no KDAL code dependency and no request-by-request KDAL ABI. ELMOS is not a
guest service or admission authority. The arrows describe orchestration and
containment, not circular source/library dependencies.

## Missing Linux guest capability

The local source candidate `/Users/user/kdal-qemu/kernel/linux-6.6.80`
exists, but has no `.config`, built `Image`, generated release header or
`Module.symvers`. No matching KDAL module is in its shared module directory.
[guest-input-inventory.json](guest-input-inventory.json) pins inspected files
and absence. It is not a Git checkout and its directory name is not a verified
source-archive checksum. The small existing qcow2 file is not boot evidence.
No Linux or KDAL guest was built or executed in this task.

HAVEN's `src/core/partition.c::partitions_launch` currently launches fixed demo
entry IPAs with argument zero. `arch/arm64/partition.S` sets EL1h/DAIF and a
stack but does not establish the complete Linux entry state. `haven_init`
ignores the firmware DTB. Linux requires a valid admitted guest DTB and image
placement/state per its boot protocol. See the local pinned boot document
hash in the inventory and [official AArch64 boot documentation](https://docs.kernel.org/arch/arm64/booting.html).
In the initial slice, use an uncompressed Image with a built-in initramfs;
honor the actual Image header rather than assuming the demo's zero offset.

Linux needs a GIC frontend and a working architectural timer to reach a
normal PID1/module environment. Existing LR injection provides neither a
virtual distributor/redistributor nor complete timer delivery. The proposed
choice is a bounded virtual GICv3 frontend for the pinned UP configuration,
with implemented LR limits and only admitted events. Mapping the physical
GICD wholesale is rejected: stage-2 page permissions cannot constrain control
bitfields for unassigned IRQs/CPUs. Required MMIO accesses and timer/EOI state
transitions must be traced and reviewed before implementation.

The guest's CNTV/PPI27 path is distinct from HAVEN's CNTHP/PPI26 scheduler.
The former is a Linux prerequisite; multicore CNTHP policy and CPU2 PPI
configuration can remain deferred. Likewise the current generic stage-2
handler's instruction skipping is not Linux MMIO emulation or a general
guest-abort contract. Recognized virtual device accesses require explicit
emulation; all other denied accesses require a defined fail-closed action.

Console PL011 is an initial diagnostic exception: the current HAVEN logger
and demo share it. A Linux console must not be advertised as exclusively
isolated while shared. The negative test therefore uses an external debugger
oracle, not forgeable guest console lines. A separate trusted console or
reviewed multiplexing is required before stronger console/device ownership
claims. KDAL's software `uart0` is not that PL011 device.

## KDAL source prerequisites

| ID | Source trace / result | Smallest proposed treatment |
| --- | --- | --- |
| K1 | `kdal_attach_driver` calls probe before assigning `device->driver`; static `uart_device` has no association; `uart_probe` dereferences `device->driver->priv`. Registry registration does not populate it. I2C/SPI/GPU use the same pattern. Definite source-order defect; no captured module-load panic. | Separate KDAL review: establish a probe-visible association and restore it on failure, or pass explicit context. Validate existing probes, successful attach, injected failure cleanup and unload independently of HAVEN. |
| K2 | `Makefile` conditionally includes QEMU/example objects, but core init calls them unconditionally. The default KDIR is a Linux-host path; `scripts/env/kernel.sh` treats Apple ARM64 as native and prints a nonexistent `module` target. | ELMOS uses explicit `ARCH=arm64`, valid cross-toolchain and `CONFIG_KDAL_QEMU_BACKEND=y CONFIG_KDAL_EXAMPLE_DRIVERS=y` against the exact kernel build. Use the actual `modules` kbuild entry. Do not execute historical setup scripts blindly. General optional-config cleanup is separate. |
| K3 | UAPI reports 0.1.0; fixed-width fields but no `compat_ioctl`. `/dev/kdal` mode0666; SELECT_DEV validates names, not HAVEN permissions. | Initial client is AArch64 only; pin UAPI header hash and module/kernel compatibility. Invalid name is an API negative, not authorization evidence. No new per-request EL2 call. |
| K4 | Chardev is published before backend/driver readiness. Open fds retain device pointers and registry finders do not establish a lifetime ref. | Serialized, non-hostile PID1 fixture opens only after successful `insmod`, closes all fds before unload; test load/operation/unload/reload. Live concurrent removal/hotplug is outside this slice and must not be claimed safe. |

The proposed trusted resource boundary assumes even a compromised Linux/KDAL
guest cannot access excluded memory or devices. KDAL's fd semantics and Unix
permissions govern ordinary in-guest use; they are not the hypervisor security
boundary. A physical backend must later fit admitted MMIO, IRQ and DMA mappings.
For the software ring there is no DMA initiator to isolate.

The independent Isolation Guardian reviewed this placement, source blockers,
and dependency choices. Its findings were incorporated above. This is a design
review, not approval to implement the virtual GIC or a KDAL lifecycle change.
