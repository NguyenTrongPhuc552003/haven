# Stage 0: component boundaries and interrupt probes

Date: 2026-10-25. Classification: AUDIT / EXPERIMENT / DOCUMENTATION.
HAVEN owns this evidence package. Stage 0 remains in progress. The scope is
macOS-hosted ARM64 QEMU, with no physical boards, Linux guest under HAVEN,
KDAL integration, DMA exercise, or timing guarantee.

## State recovered and preservation completed

Revisions: HAVEN `f775304ce9443dc9861ba1880e4208c7ae19b314`, ELMOS
`5ef5cd7fb974fca961e1af7f5a7f08ff47f70e95`, KDAL
`1ab0dce6dedada4162325ba2101db63214ed896c`. The only pre-existing tracked
HAVEN difference is the fatal ESR/FAR/ELR print, saved verbatim as
[session-start-haven.patch](session-start-haven.patch). ELMOS and KDAL were
clean. Existing manuscript PDF changes remain untouched; wiki unchanged.
[repository-state.json](repository-state.json) records all five revisions and
status, captured after creating this evidence directory. The workspace root
has no Git repository.

Completed before this task: host inventory/ELMOS build and doctor; direct
QEMU HVC failure localization; TF-A/SMC guest A launch; CPU 2 full-init versus
vectors-only versus minimal-stage-2 experiments. Those experiments were reused,
not reclassified as production behavior.

All 41 legacy files from the five `build/evidence/stage0-*` directories were
copied into [legacy-evidence.tar.gz](legacy-evidence.tar.gz). Every archived
member was read back and its SHA-256 compared with the original. The filenames,
sizes and hashes are in [legacy-inventory.json](legacy-inventory.json).
No legacy file was changed or removed. The archive includes the invalid first
TF-A trial that omitted the guest loaders; it remains invalid for guest claims.

New raw runs follow the existing `build/evidence/<campaign>` convention, at
`build/evidence/stage0-20261025-boundaries/`. They are also frozen in
[experiment-evidence.tar.gz](experiment-evidence.tar.gz), outside ignored build
paths, with 248 individually hashed files in
[experiment-inventory.json](experiment-inventory.json). This archive was also
read back and hash-verified. It includes logs, images, ELF/disassembly for the
faults, exact source patches, per-run commands/results, configure/build logs,
CMake caches and compile commands. The archive and metadata in this directory
are Git-visible but **not committed or remotely backed up** by this task.

The existing `scripts/evidence/package-evidence.sh` convention was inspected;
it runs broad CI/smoke/tests and overwrites aggregate files, so it was not used
for a preservation-only operation. Its evidence-directory, metadata, structured
summary and hash-manifest conventions are retained here. No release upload or
new implementation repository was created.

## Reproduction and provenance limits

Firmware images are the preserved TF-A v2.15.0 binaries attributed by the
original capture to commit `da738d5eae93af342fdc4995dd3c05acb4c9d757`.
The deleted original TF-A checkout cannot be inspected or rebuilt in this task;
firmware byte identity was checked against the preserved images. Historical
source-change descriptions do not reconstruct the exact deleted secondary
probe trees. The original complete launch command was not preserved, so the
new replay command is recorded as a reconstruction, not an original transcript.

The new experiments use the retained firmware directly. They require QEMU,
Python 3 with tarfile's data filter, CMake, Ninja, the AArch64 GNU toolchain,
and the pinned HAVEN revision in the supplied Git object database. No package
installation or firmware download was needed. Host/tool versions and exact
queries are in the new archive's `host-tools.json`.

Run the final diagnostic suite from the workspace with a **fresh**, unique work
path (the runner skips existing named run directories):

```sh
python3 haven/docs/methodology/stage0/20261025-boundaries/run_probes.py \
  --haven haven --work /private/tmp/haven-stage0-boundaries-replay
```

`run_probes.py` exports the pinned revision, applies the saved session diagnostic,
then changes only that disposable source tree. It records exact source patches
for each image. `probe.c` and `guest_probe.S` are diagnostic fixtures; they are
not linked into the tracked HAVEN target. The final runner reproduces the final
seven probe/control variants and manual A replay. Historical failed iterations
remain in the archive with their own patches and build configuration. To
reproduce one exact historical source variant, export the pinned revision and
apply that run's `source.patch` with `git apply`; use its recorded configure
arguments with fresh source/build paths. The strict-alignment failed build uses
the preceding aligned-data patch plus `-DCMAKE_C_FLAGS=-mstrict-align`.

Build timestamps embedded by HAVEN (`__DATE__`, `__TIME__`) mean a later source
rebuild need not hash identically. Exact-byte replay is possible with the
archived images; equivalence of a rebuild must be checked by source patch,
compiler/configuration and markers, not assumed from the old image hash.

Every QEMU process uses this reconstructed argument structure (relative image
paths resolved inside each recorded `cwd`):

```sh
qemu-system-aarch64 \
  -machine virt,secure=on,virtualization=on,gic-version=3,iommu=smmuv3 \
  -cpu cortex-a72 -smp 4 -m 2G \
  -display none -monitor none -serial file:uart.log \
  -bios bl1.bin -semihosting-config enable=on,target=native \
  -device loader,file=haven.bin,addr=0x80000000,force-raw=on \
  -device loader,file=guest_a.bin,addr=0x80800000,force-raw=on \
  -device loader,file=guest_b.bin,addr=0xa0800000,force-raw=on
```

`bl2.bin` and `bl31.bin` must exist in that cwd for semihosting. No `cpu-num=0`
is set on the HAVEN loader: TF-A starts at BIOS and hands off to preloaded BL33.
The direct-loader historical experiment instead set CPU 0's PC using the loader.
See [QEMU generic loader](https://www.qemu.org/docs/master/system/generic-loader.html)
and [TF-A QEMU boot guide](https://trustedfirmware-a.readthedocs.io/en/latest/plat/qemu.html).

The host watchdog is four seconds, then SIGTERM with a two-second kill fallback.
QEMU's signal exit is recorded separately from the marker verdict. A timeout
alone is never a pass. The diagnostic guest polls the physical counter for
100 ms of guest counter time, then reports by HVC. This is an observation window,
not a latency measurement. A missing result, fatal marker, wrong CPU/ID/count,
failed owner check, or uncompleted EOI is a failure. Build commands each have
120-second bounds. Configure/build failures stop that invocation.

## Probe changes and results

All new variants temporarily use SMC and call the full existing CPU initializer
on CPU 2, followed by virtual-interface setup and a masked CNTHP initialization.
The probe explicitly permits EL1 counter reads and initializes CNTVOFF. It logs
CPU=2 and CurrentEL=2 on entry to its C setup. This is an observation after the
assembly stack/mailbox path, not validation of that path for arbitrary firmware.

For physical delivery, diagnostic code additionally initializes ICC_PMR,
ICC_BPR1, ICC_CTLR (EOImode=0) and ICC_IGRPEN1, and registers an IRQ callback
that reads IAR, checks the known source, masks/disables it, EOIs and returns.
These actions are **missing from the ordinary production path**. The guest
installs its EL1 vector, enables the virtual system-register interface, unmasks
IRQ, acknowledges/EOIs a virtual IRQ in an actual exception handler, and reports
its count after resuming. Physical IRQs are taken through HAVEN's existing
lower-AArch64 IRQ vector while B executes. Current-EL2 IRQ vectors were not
changed or exercised.

[run-summary.json](run-summary.json) indexes exact argv/cwd, image hashes,
timeouts and marker verdicts. In every final passing variant A also retained
its greeting, deliberate denied load and post-fault marker. B is replaced by a
diagnostic guest; its original denied-load workload was not rerun in this suite.

| Run / claim | Explicit expected condition | Observed result | Scope / limitation |
| --- | --- | --- | --- |
| `replay-a` | Preserved manual images produce A post-fault marker within 4 s | PASS; original firmware banner, A greeting, denied IPA `0x60000000`, resumed marker | Independent manual baseline; no ELMOS launcher |
| `virtual`, mode 0 | One actual guest IRQ with vINTID 60; EOI and resume; LR0 invalid afterward | PASS; guest=1, vintid=60, LR0 state=0, no physical IRQ | Uses architecture injector directly, bypasses public policy injection API |
| `virtual-disabled`, mode 1 | Same pending LR with ICH_HCR.EN=0 yields no guest IRQ | PASS control; guest=0, LR0 remains pending | Full CPU init already enables ICH; omitting only the later setup call would not be a valid disabled control |
| `physical-spi-scalar`, mode 2 | Assign synthetic SPI 40 to partition 2/CPU 2; reject partition 1 reassignment without route change; receive exactly once and resume | PASS; assign=0, denied=HV_EPERM (-1), IROUTER before/after=2, CPU2 IAR=40, count=1, active=0 | Software-pended physical SPI, no peripheral; custom callback; assignment rejection is not proof of integrated guest delivery policy |
| `timer-detail`, mode 3 | CPU2 PPI26 enabled, two 10 ms future compares, rearm once, then mask; guest resumes | PASS; two CPU2 IAR=26 events, active=0, final timer masked | Register-level CNTHP test; no scheduler/deadline policy validation |
| `timer-masked-detail`, mode 4 | Expired compare with IMASK=1 produces zero IRQs | PASS control; count=0, CTL=7 (enabled, masked, expired) | Timer source masking only |
| `timer-future`, mode 6 | Compare 2 seconds ahead must not expire during 100 ms window | PASS control; count=0, CTL=1 (enabled, unmasked, not expired) | No wall-clock/real-time claim |
| `timer-original-ppi-detail`, mode 5 | Exercise unchanged PPI API on CPU2; record selected redistributor and delivery | **Production behavior FAIL**, expected-negative probe PASS: CPU0 enable=`0x40000ff`, CPU2=0, count=0, CTL=5 (expired/unmasked) | Localizes wrong-CPU PPI enabling to HAVEN's GIC API; no ELMOS/KDAL change warranted |

The earlier `timer`, `timer-masked`, and `timer-original-ppi` runs had the same
outcomes; the `*-detail` repeats added explicit current-counter/CVAL logging.
Example positive timer values: frequency 62,500,000 Hz, first now=4,195,250,
CVAL=4,820,250; second now=4,993,812, CVAL=5,618,812. Both comparisons are
625,000 ticks ahead. No statistical timing experiment was performed.

Readback after CPU setup: HCR=`0x80080039`, VTCR=`0x80023559`, VBAR=`0x80000800`,
ICC_SRE_EL2=`0xf`, ICH_HCR=1, ICH_VMCR=`0xff4c000a`; ICH_VTR reports **4 LRs**.
The existing injector succeeds in LR0; its hard-coded scan of 16 LRs is still
unsafe as a general capacity contract. Inherited physical ICC_IGRPEN1=0 and
ICC_PMR=`0xf8` were recorded before diagnostic CPU-interface programming.

## First-failure analysis

1. `physical-spi`: first failure was a 64-bit literal load from four-byte-aligned
   `0x80005d8c` in `stage0_prepare`, before assignment. ESR=`0x96000021`,
   ELR=`0x8000146c`; saved disassembly identifies `ldr x2,[x0]`. The independent
   replay and other probes booted. Immediate owner: diagnostic fixture; probable
   enabling issue: bare-metal compiler/MMU alignment contract in HAVEN.
2. `physical-spi-aligned`: changing diagnostic aggregates to aligned static
   objects moved existing `exception_enabled` to `0x80006fdc`. Initialization
   then faulted earlier at the existing `hv_el2_exceptions_init`'s `stp x1,x1`
   (`ELR=0x800038ac`, ESR=`0x96000061`). This exposes layout-sensitive existing
   HAVEN code, not a firmware transport or ELMOS failure. Both failed images,
   source patches and disassemblies are retained.
3. A disposable `-mstrict-align` build reached link then failed on compiler-
   generated `memcpy` from `hv_stage2_map_partition`; the freestanding runtime
   exports `hv_memcpy`, without this ABI symbol. The full failed command/log and
   same source patch are retained. No strict-alignment production fix was adopted.
4. Final physical probe used volatile scalar route-field assignments on the
   stack, avoiding aggregate literal copies and initialized-data layout changes.
   It passed with the normal compiler flags. This isolates interrupt behavior;
   it does **not** resolve the separate HAVEN compiler/MMU/runtime ABI gap.
5. The original PPI API enabled redistributor 0 while CPU 2's compare expired.
   The positive variant changed only the diagnostic PPI enable target to CPU 2,
   with the same service fixture. This is direct evidence for HAVEN ownership of
   that failure. No compensation in another repository was attempted.

## Reviewed contract corrections and readiness

The independent Isolation Guardian review and probes supersede these parts of
the earlier candidate contract:

- Full `hv_arch_cpu_init()` is an inventory/start point, not a complete production
  contract. It does not establish a declared EL2 translation/cache handoff,
  physical CPU-interface state, LR/active-priority reset, per-CPU timer ownership
  or service callbacks. The minimal stage-2 subset remains insufficient evidence.
- `hv_arch_cpu_init` already enables the virtual interface with a VMCR default
  different from the later `hv_arch_gic_el2_setup`. Give per-CPU hardware init a
  single explicit order. Do not call `hv_el2_exceptions_init()` on CPU 2: it also
  clears global routes, handlers and counters.
- `secondary_entry` uses Aff0 for stack/mailbox without bounds or entry-EL checks;
  other CPU helpers encode affinity differently. Declare SP selection, masks,
  MMU/cache state, full MPIDR mapping and memory publication/acknowledgement.
- PSCI error currently suppresses mailbox release and prints B inactive; it
  does not update a lifecycle state. Success is not a secondary-ready signal.
  SMC remains a property of the measured TF-A path, not a global board choice.
- `entry.S::el2_spx_irq` is fatal and does not construct valid fatal arguments.
  `hv_handle_irq` invokes only an optional global callback; no production caller
  registers the required service. Positive probes used that missing callback.
- `hv_el2_inject_exception(partition,...)` checks argument ranges but never
  checks owner/current partition. `hv_el2_route_irq` and `hv_irq_assign` maintain
  different tables, and IRQ dispatch does not enforce their joint policy.
  Virtual LR success must not be described as ownership enforcement.
- Timer policy scans a global partition table and programs the calling CPU's
  CNTHP, with no established CPU binding. Register-level timer success does not
  validate partition deadlines or budgets.
- The GIC driver uses constants described in the Secure GICD_CTLR view from
  non-secure EL2. The tested interrupt works with this firmware, but the security
  view and firmware ownership still need an explicit audit before generalizing.

**Readiness:** the narrow PSCI/secondary-startup and wrong-CPU PPI defects have
sufficient evidence for correction review. The previous combined proposal is
**not ready for adoption** as a complete IRQ/timer initialization fix. Split it
into reviewable HAVEN changes with the acceptance criteria in
[BOUNDARIES.md](BOUNDARIES.md). No temporary SMC, CPU init, handler or compiler
change was ported into tracked implementation.

Official architectural context: [Arm GICv3/v4 software overview](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/GICv3_v4_overview.pdf),
[Arm Generic Timer guide](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Generic%20Timer.pdf),
and [PSCI DEN0022E](https://documentation-service.arm.com/static/640f584656ea36189d4e94a4).
Source symbols, the archived instruction stream and observations are the basis
of the local findings; architecture documentation alone is not test evidence.
