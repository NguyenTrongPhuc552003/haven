# One proposed integrated acceptance scenario

ID: `linux-kdal-ring-isolation-v1`. Status: specified, **not executed**.
Dependencies: [contracts C1–C7](CONTRACTS.md) and M0–M2 in
[ROADMAP.md](ROADMAP.md). The scenario is a first reproducible interface and
CPU-memory containment result; it is not the final research evaluation.

Use one UP AArch64 Linux guest at EL1 on CPU0 under HAVEN. ELMOS prepares,
builds and hashes the full artifact set, launches pinned QEMU/TF-A, drives the
known initramfs fixture and captures results. KDAL provides its real existing
`/dev/kdal` → UART driver → software ring abstraction. HAVEN admits the guest's
resource envelope and denies an excluded scratch page. Normal KDAL I/O needs
no hypercall and changes no HAVEN policy.

## Preflight and checkpoints

| Checkpoint | Required observation / failure condition |
| --- | --- |
| P0: artifacts | Known kernel/config/module/UAPI/guest DT/profile/test/firmware hashes and compatible version1 manifest. Missing input, bad Image placement, mismatched module or unapproved profile fails before spawn. |
| P1: independent controls | The same kernel and KDAL artifacts already passed direct Linux-QEMU functional baseline. The typed ELMOS launcher already reproduced the preserved TF-A/HAVEN bare-metal reference. A failed control blocks the integrated run. |
| P2: firmware/HAVEN | TF-A entry → HAVEN EL2 → compiled profile/hash/resource admission. No CPU2 launch for this profile. A wrong EL or unsupported resource cannot proceed to guest entry. |
| P3: Linux | Correct guest DT/IPAs and entry state, virtual GIC/counter/timer functioning, PID1 readiness within60s and matching artifact/run identity. A boot banner alone does not pass this checkpoint. |
| P4: KDAL | Successful `insmod`, version0.1.0, expected `uart0` enumeration and info, no kernel oops. `/dev/kdal` creation before driver readiness is insufficient. |
| P5: one operation | Open fd, SELECT_DEV `uart0`, write exactly32 nonce-bearing bytes, read exactly32 and compare bytes/digest/counts. Fail on short/unexpected result, stale data or timeout. This is software device I/O within guest RAM. |
| P6: API controls | A separate unselected fd rejects I/O with ENODEV; unknown device name rejects selection with ENODEV without changing an existing selection. With a valid selected powered-on device and nonzero byte count, a bad user pointer yields EFAULT; power-off write yields EIO and restoring power allows normal I/O. These are KDAL behavior checks, not partition authorization. Close all descriptors. |
| P7: actual unauthorized operation | A known HAVEN-owned Linux probe module maps the excluded test IPA and executes one identified store to the reserved canary page. HAVEN stops it at the expected stage-2 denial. External GDB captures CPU/EL/fault PC/IPA/ESR and unchanged physical canary. Wrong checkpoint, skipped instruction, store completing, changed canary or absent external oracle fails. |
| P8: cleanup | Host records expected terminal guest condition separately from watchdog/exit status, terminates/reaps QEMU and preserves all files. A lingering process or lost logs fails cleanup even if earlier checks succeeded. A fresh VM is the recovery mechanism. |

The expected Linux/runtime watchdogs and process termination sequence are in
C1–C7; all exact values are stored in the manifest. No unbounded interactive
login, SSH, cloud-init, disk, network, 9p mount or external service is needed
for this first initramfs scenario. Those capabilities can be added only if a
later workload needs them.

## Necessary denial control

Run the terminal scratch-page probe in two separate fresh VMs with identical
software/nonce and only the scratch admission changed. In the allowed control,
HAVEN maps that reserved test page and the exact store must change its canary.
GDB stops at a known post-store checkpoint before reading that control's
physical canary; an absent checkpoint is not an allowed-control pass.
In the denied case, the mapping is absent and the canary must stay unchanged
at the precise abort. No real device register, EL2 text or firmware memory is
used as a target. This detects a probe that never executed or an oracle reading
the wrong physical page. The allowed test profile never becomes a production
permission fallback.

The probe is test-only code under HAVEN's test ownership, compiled as an EL1
Linux helper; it is not a production KDAL backend or a new EL2 operation ABI.
Its unknown-address access must be distinguishable from emulated vGIC MMIO.
The existing generic “skip the denied instruction” handler cannot satisfy the
terminal denial criterion unchanged.

## What each result proves

| Evidence | Supported conclusion | Excluded inference |
| --- | --- | --- |
| ELMOS manifests, command/process capture and repeated fresh launch | Reproduction of the pinned combined artifacts and checkpoints | All macOS/toolchain/QEMU versions supported |
| Linux PID1 and matching KDAL init | Required execution environment is booted under HAVEN | General Linux distribution, SMP, hotplug or arbitrary drivers supported |
| `/dev/kdal` exact byte round trip | Client, chardev, driver/backend and ring cooperate inside the guest | PL011 UART hardware, accelerator, physical device or DMA isolation |
| KDAL negative errno controls | Implemented API selection/buffer/power error behavior | HAVEN resource admission or malicious-guest containment |
| External allowed/denied scratch oracle | The specified guest-EL1 CPU store is allowed/blocked by the tested stage-2 configuration | Complete device/IRQ isolation, read/fetch denials, DMA isolation or all attacks covered |
| Console/vGIC/timer functional checks | The bounded Linux guest support path works for the pinned profile | Predictable execution, scheduler budgets or real-time guarantees |

Report every checkpoint and control as PASS, FAIL, BLOCKED or NOT RUN with its
first divergence. Today this integrated scenario is BLOCKED at P0/M1 because
the matched guest artifacts and HAVEN Linux/virtual frontend are absent, and
at KDAL functional readiness pending K1. The new ABI/alignment experiments are
independent M0 evidence; they do not pass any P2–P8 integrated checkpoint.
