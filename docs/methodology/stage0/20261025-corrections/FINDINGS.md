# Stage 0 finding ledger

Date: 2026-10-25. Classification: AUDIT / DOCUMENTATION, with isolated compiler
validation. Owner: HAVEN. Base revision:
`f775304ce9443dc9861ba1880e4208c7ae19b314`. Local differences are recorded in
[repository-state.json](repository-state.json) and
[session-start-haven.patch](session-start-haven.patch). Findings below refer to
that revision unless explicitly identified as an isolated proposal result.

The evidence distinguishes a captured failure, a source contract gap, and an
unresolved explanation. A diagnostic prerequisite is not automatically a
production correction. The dependency order and first proposal are in
[REVIEW.md](REVIEW.md).

## Source and evidence keys

| Key | Exact source or evidence                                                                                                                                                                                                                                                                                                                    |
| --- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| B   | [boot.S](../../../../arch/arm64/boot.S): `_start`, `secondary_entry`, `hv_arch_secondary_start`, `hv_arch_psci_cpu_on`; [partition.c](../../../../src/core/partition.c): `partition_b_secondary_cpu_init`, `partitions_launch`; [cpu.c](../../../../arch/arm64/cpu.c): `hv_arch_cpu_init`, CPU affinity helpers                             |
| I   | [entry.S](../../../../arch/arm64/entry.S): `lower_el64_irq`, `el2_spx_irq`; [irq.c](../../../../arch/arm64/irq.c): `hv_arch_gic_el2_setup`, both LR loops; [el2_exceptions.c](../../../../src/core/exc/el2_exceptions.c): init, route, injection and IRQ handling; [ownership.c](../../../../src/core/irq/ownership.c): assign/revoke/check |
| T   | [gic_v3.c](../../../../drivers/irqchip/gic_v3.c): redistributor addressing, enable/disable and IAR/EOI; [arch timer](../../../../arch/arm64/timer.c): CNTHP register operations; [policy timer](../../../../src/core/time/timer.c): global table and earliest-deadline programming                                                          |
| A   | [flags.cmake](../../../../cmake/flags.cmake), [string.c](../../../../src/common/string.c), [string.h](../../../../include/haven/string.h), [linker.ld](../../../../linker.ld), `hv_arch_cpu_init`, `hv_stage2_map_partition`                                                                                                                |
| L   | Preserved [legacy archive](../20261025-boundaries/legacy-evidence.tar.gz), SHA-256 `3b1be85367d901b83780baa789dd3b22ca9d7e6a1c517759d56c5ffd9bd90f33`; paths below are archive member paths                                                                                                                                                 |
| E   | Preserved [experiment archive](../20261025-boundaries/experiment-evidence.tar.gz), SHA-256 `b4eb470e2357b0a0fc3e5b24ab2147551b8d4676d98e222da28d9bc44fa69e4b`; each run has `run.json`, `source.patch`, images and `uart.log`                                                                                                               |
| V   | New [validation archive](validation-evidence.tar.gz), mirrored under ignored `build/evidence/stage0-20261025-corrections/`; [validation summary](validation-summary.json) and [inventory](validation-inventory.json) identify exact inputs, commands and outputs                                                                            |

Official contracts used: Arm [PSCI DEN0022E](https://documentation-service.arm.com/static/640f584656ea36189d4e94a4),
§§3.4, 5.2, 5.6 and 6.4; Arm [GIC overview DAI0492 v3.0](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/GICv3_v4_overview.pdf),
interrupt state, initialization and virtualization sections; Arm
[Generic Timer 102379_0102_01](https://documentation-service.arm.com/static/651fbd69bc48b0381ce0e06c),
timer registers, interrupt assignments and masking; [TF-A v2.15.0 QEMU guide](https://trustedfirmware-a.readthedocs.io/en/v2.15.0/plat/qemu.html);
GCC [freestanding runtime requirements](https://gcc.gnu.org/onlinedocs/gcc/Standards.html)
and [AArch64 alignment options](https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html).
These define obligations and interfaces; the archive logs establish observations.

## 1. PSCI conduit and secondary startup

### B1 - Conduit chosen without a firmware service contract

- **Expected contract / source:** B and PSCI §§3.4/5.6. Select a declared
  firmware service. CPU_ON64 uses ID `0xC4000003`, target affinity in x1,
  physical entry in x2, context in x3, and a status result. The measured TF-A
  service is at EL3. A direct-loader profile has no established PSCI provider.
- **Observed / evidence:** L `stage0-20261023-qemu-diag1/qemu-uart-diag1.log`
  records ESR `0x5a000000`, ELR `0x800000e8`, immediately after EL2's HVC.
  L `stage0-20261023-tfa-qemu-guest1/qemu-uart.log` and E
  `replay-a/uart.log` reach A's greeting, denial and resume with temporary SMC.
- **Cause / confidence:** confirmed mismatch between HAVEN's unconditional HVC
  and its EL2 caller. HVC targets EL2; it cannot request the measured EL3 service.
  The comment equating every QEMU launch with an HVC conduit is incorrect.
  Absence of an observed direct-load provider is not proof that all QEMU
  configurations lack PSCI.
- **Dependencies:** profile design is independent; runtime adoption needs A1.
  B2/B3 determine whether a successful request can launch a safe secondary.
- **Smallest correction:** explicit QEMU boot profiles: direct load declares
  unavailable CPU_ON; the pinned TF-A profile selects SMC. Validate the profile
  before calling, preserve signed status and arguments, and report unsupported
  service without executing a speculative HVC/SMC. Do not globally replace HVC.
- **Positive / negative regression:** TF-A profile emits the intended instruction
  and argument values; CPU_ON acceptance and secondary-ready are separate
  checkpoints. Direct profile makes no firmware call and reports B unavailable.
  Invalid profile and nonzero firmware statuses cannot release a mailbox or
  announce readiness. Use a controlled transport stub for statuses, followed by
  a real firmware test; a stub result is not a firmware observation.
- **Unverified:** other firmware, PSCI feature discovery, physical targets,
  all error paths and the new profile implementation.
- **Decision 2026-10-31 (owner, E2):** MM1/MM2 QEMU tests use the TF-A profile
  with SMC, implemented as the explicit-profile correction above. See the
  [QEMU readiness audit](../../stage1/20261031-mm1-qemu-readiness/AUDIT.md).
  Implemented 2026-10-31 (uncommitted): `HAVEN_BOOT_PROFILE` qemu-direct
  (no firmware call) / qemu-tfa (SMC); pinned GCC 15.2.0 builds of both
  link. **Observed 2026-10-31 on QEMU 11.1.3 (direct profile, cortex-a72 and
  max):** CPU_ON reports NOT_SUPPORTED, partition B inactive, no fatal
  exception, partition A runs and its cross-partition read is denied. TF-A
  profile not run. See the
  [boot-profile BUGFIX](../../stage1/20261031-boot-profile-bugfix/README.md).

### B2 - Secondary guest entry omits per-CPU initialization

- **Status (2026-11-02):** resolved for QEMU in haven `9102099` (owner
  approval "finish partition B"). Observed before the fix with `-d int`:
  illegal exception return at EL2 (HCR_EL2.RW = 0), then an endless
  undefined-instruction loop on CPU 2 at VBAR_EL2 = 0; B never ran. After
  the fix CPU 2 runs `hv_arch_cpu_init()` and `hv_arch_phys_irq_cpu_init()`
  before stage-2 activation; partition B's denial is logged from CPU 2 and B
  takes its own timer interrupt. Control K2 (setup removed) fails as
  expected. Record: `docs/methodology/stage2/20261102-partition-b/`. Still
  open from the regression list below: a ready acknowledgment (B3) and a
  complete virtual-state reset (I1).

- **Expected contract / source:** `hv_arch_cpu_init` explicitly requires one
  invocation per CPU before guest activity. Architecturally, register state is
  per PE; firmware startup state does not replace HAVEN's chosen guest state.
  PSCI §6.4 defines return-level and reset/handoff conditions.
- **Observed / evidence:** tracked `partition_b_secondary_cpu_init` activates
  VTTBR and ERETs without that initializer. L
  `stage0-20261024-tfa-qemu-secondary/vector-only/qemu-uart.log` fails at B's
  entry; `full-cpu-init/qemu-uart.log` and `minimal-s2/qemu-uart.log` record two
  B denials. E `virtual/uart.log` observes CPU2 EL2 and guest IRQ delivery only
  after additional diagnostic setup.
- **Cause / confidence:** missing secondary EL2 setup is confirmed by source
  and controlled comparisons. The experiments do not identify every required
  register. Full init also lacks an explicit memory handoff (A1), physical ICC
  service state (I2) and complete virtual state reset (I1).
- **Dependencies:** A1, B1, B3; I1/I2/T1 are required before corresponding IRQ
  or timer readiness may be advertised.
- **Smallest correction:** define one masked per-CPU preparation path and call
  it before stage-2 activation/guest entry on both CPUs. Keep shared policy-table
  reset in primary-only initialization. Define which readiness level is reached;
  stage-2 readiness must not imply IRQ/timer service readiness.
- **Positive / negative regression:** original A workload and diagnostic B
  allowed/denied memory operations on their assigned CPUs; readback of the
  specified state, ready acknowledgment after initialization, no global table
  reset on CPU2. A skipped-init test image must fail the readiness condition.
  IRQs stay masked until their service prerequisites pass.
- **Unverified:** cache/MMU transitions, complete guest context, repeated
  startup/reset, other topologies and physical firmware. The smallest tested
  stage-2 register subset is not the production contract.

### B3 - Affinity, stack bounds, publication and readiness are implicit

- **Expected contract / source:** B; PSCI §6.4 supplies context and startup
  state, while HAVEN owns logical CPU mapping and safe memory publication.
- **Observed / evidence:** `secondary_entry` discards firmware context, indexes
  stacks/mailboxes using Aff0 without bounds or EL checks, and branches through
  a polled entry word. `hv_arch_cpu_id` instead combines Aff1 and truncated Aff0;
  the MPIDR helper drops Aff3. No ready acknowledgment/timeout or lifecycle
  update follows CPU_ON. E `virtual/uart.log` confirms CPU2 only after the
  unchecked assembly path. The primary's “woken” log is not ready evidence.
- **Cause / confidence:** confirmed source gaps for a general contract; no
  out-of-bounds access, wrong-affinity boot or publication race was captured.
  The measured single-cluster QEMU topology happened to fit these assumptions.
- **Dependencies:** A1 for memory visibility, B1 for service; B2 consumes the
  validated CPU index. Later PPI and IRQ targeting consume the same mapping.
- **Smallest correction:** initially constrain and validate the measured QEMU
  topology, or provide one explicit affinity map. Check entry EL and bounds
  before stack/mailbox use; declare SP selection and masks. Specify release/
  acquire publication plus cache handling for the chosen handoff. Add a bounded
  startup state/acknowledgment; a late CPU must not enter a failed partition.
- **Positive / negative regression:** CPU2 obtains its own aligned stack and
  mapped identity; invalid affinity/EL/index parks before indexed access;
  nonzero CPU_ON, delayed/absent ready, and late ready cannot cause guest entry.
  Preserve A's behavior under the explicitly selected failure policy.
- **Unverified:** lifecycle implementation, timeout cancellation protocol,
  publication stress and multicore concurrent initialization. Existing host
  `test_secondary_cpu_isolation` excludes PSCI/assembly; its policy rejection
  tests cannot validate these firmware failure cases.

## 2. IRQ dispatch, service and ownership

### I1 - Virtual interface capacity and initialization assumptions

- **Expected contract / source:** I and the GIC virtualization interface.
  Discover implemented LR capacity through `ICH_VTR_EL2`; use only supported
  registers and establish known virtual pending/active state before enabling.
- **Observed / evidence:** E `virtual/uart.log` reports four LRs; injection
  succeeds in LR0. `virtual-disabled/uart.log` retains one pending LR and
  delivers zero IRQs with ICH_HCR.EN clear. Both `hv_arch_inject_virtual_irq`
  and `hv_arch_pending_virtual_irqs` scan sixteen. CPU init and later GIC setup
  write different VMCR defaults and do not explicitly reset all LR/AP state.
- **Cause / confidence:** confirmed invalid capacity assumption and duplicated
  initialization ownership. No access to an unimplemented LR was captured:
  the positive test used the first free slot. Inherited-state leakage is a
  risk requiring tests, rather than a demonstrated leak.
- **Dependencies:** A1 for runtime validation; B2 for per-CPU call order.
  Capacity limiting can be reviewed as its own leaf correction.
- **Smallest correction:** first bound both loops to implemented capacity;
  reject unusable capacity consistently. Separately consolidate per-CPU virtual
  reset/order, including supported active-priority registers, before enabling.
- **Positive / negative regression:** fill exactly four LRs in a four-LR model;
  fifth enqueue returns `HV_ENOSPC` without unsupported register access or LR
  mutation. Count, EOI/reuse and EN-disabled controls must agree. Dirty initial
  LR/AP fixtures cannot enter a guest until reset completes.
- **Unverified:** saturation, maintenance IRQs, other capacities, migration,
  nested virtualization and hostile inherited state.

### I2 - Production physical IRQ service is incomplete

- **Expected contract / source:** I/T and GIC interrupt lifecycle. Initialize
  the receiving PE's physical CPU interface, preserve interrupted context,
  acknowledge a valid ID, quiesce/service its source, and complete the chosen
  priority-drop/deactivation mode before return. Keep sources masked until ready.
- **Observed / evidence:** E `physical-spi-scalar/uart.log` receives synthetic
  SPI40 once on CPU2, resumes B and reads active state zero, using the custom
  callback in the archived patch. Initial IGRPEN1 was zero. Tracked
  `hv_handle_irq` calls only an optional handler; no production caller installs
  that service. Current-EL2 SPx IRQ calls the fatal handler without constructing
  its arguments. Shared context/counters also lack a per-CPU concurrency contract.
- **Cause / confidence:** source-confirmed missing service/interface path for
  the intended IRQ contract. The custom callback demonstrates sufficient
  diagnostic setup for one source; it does not validate a generic ISR design.
- **Dependencies:** A1, B2/B3, audit of the NS GICD_CTLR view/firmware ownership,
  I1 for virtual service, and I3 before guest delivery. T1 is needed for PPIs.
- **Smallest correction:** define service categories (HAVEN timer, owned guest
  IRQ, spurious/unowned IRQ), per-CPU interface state and mask order. Implement
  both supported current-EL2 and lower-EL entries with per-CPU context; keep
  guest injection disabled until I3. Select and test EOI mode explicitly.
- **Positive / negative regression:** synthetic SPI serviced once from guest
  and current-EL2 execution, context preserved and active state asserted zero.
  Spurious IDs must not receive an ordinary EOI; unowned/revoked sources must
  remain contained without a storm or guest injection. Timeout/missing handler
  is failure. Test level-sensitive quiescing separately from synthetic pending.
- **Unverified:** current-EL2 handling, peripheral IRQs, level storms, nesting,
  secure ownership and simultaneous CPU IRQs. Prior active-state readbacks were
  manually observed, not included in the probe's Boolean pass predicate.

### I3 - Ownership tables do not govern injection/dispatch together

- **Expected contract / source:** workspace isolation boundary, public
  exception API, I. A target partition/CPU and vINTID must correspond to an
  admitted source or explicit HAVEN-generated event before hardware mutation.
- **Observed / evidence:** E `physical-spi-scalar/uart.log` shows assignment
  conflict `HV_EPERM`, unchanged IROUTER=2. This exercises `hv_irq_assign` only.
  Source `hv_el2_inject_exception` ignores partition after range checking,
  restricts vector<32, and writes the calling PE's LR. The successful vINTID60
  probe calls the architecture injector directly. `irq_routes` is a separate
  table and neither dispatch nor injection consults the owner/target tables.
- **Cause / confidence:** confirmed source contract mismatch; no guest-triggered
  cross-partition exploit was executed. The low-level injector is a mechanism;
  it cannot supply missing target/authorization information by itself.
- **Dependencies:** B3 CPU/partition identity; I1 capacity; I2 dispatch for
  physical sources. Ownership design can be reviewed before those implementations.
- **Smallest correction:** specify one authoritative mapping of physical or
  synthetic source, owner, target and vINTID; check it before route/inject and
  revoke pending state consistently. Keep low-level injection private to that
  boundary. Reject unsupported remote targets until a separately reviewed
  delivery mechanism exists; do not add cross-CPU delivery as an incidental fix.
- **Positive / negative regression:** owner/current-target injection succeeds;
  wrong owner, wrong CPU, unassigned/revoked source and invalid IDs leave LRs,
  routes and success counters unchanged. Assignment rollback and pending IRQ
  revocation must be defined. Test through the public path, including a permitted
  vINTID60 if that is admitted by the revised contract.
- **Unverified:** public-path delivery, malicious guest admission, remote
  delivery and revocation of an already active IRQ.

## 3. Per-CPU PPI configuration and CNTHP

### T1 - The private-interrupt API cannot identify CPU2

- **Status (2026-11-02):** resolved in haven `9102099`.
  `gic_v3_enable_ppi(cpu, irq)` and `gic_v3_disable_ppi(cpu, irq)` target
  one redistributor (disable waits for its RWP); `gic_v3_enable_irq` and
  `gic_v3_disable_irq` take SPIs only. Ownership keys a PPI by (INTID,
  CPU). Partition B's PPI 27 is enabled on CPU 2 only (read-back: CPU 2
  PPIs [27]); control K4 (granted on CPU 0) is reported by read-back as
  enable drift on CPU 0.

- **Expected contract / source:** T and GIC per-PE redistributors. A private
  IRQ operation needs a specified PE; enable and disable must target that PE.
- **Observed / evidence:** E `timer-original-ppi-detail/uart.log`: the unchanged
  API enables CPU0=`0x40000ff`, CPU2=0; CPU2 CNTHP is expired/unmasked (CTL5)
  and count=0. E `timer-detail/uart.log`: direct diagnostic CPU2 enable gives
  CPU2=`0x4000000` and two INTID26 events. Source explicitly comments boot-CPU
  behavior in `gic_v3_enable_irq`; disable also uses redistributor0.
- **Cause / confidence:** confirmed API limitation/contract gap, not a captured
  production CPU2 timer caller defect. No production call currently enables
  PPI26: the production caller `hv_irq_assign` first invokes SPI-only routing,
  which rejects IDs<32. Changing a default CPU alone would leave timer service
  incomplete. The earlier boundary summary's “wrong-CPU defect” needs this scope.
- **Dependencies:** B3 CPU mapping; GIC redistributor affinity/stride contract;
  I2 for interrupt delivery validation. Register readback can precede ISR work.
- **Smallest correction:** explicit private IRQ enable/disable on a validated
  logical CPU, restricted to admitted SGI/PPI classes as documented. Keep SPI
  behavior separate; wait for the correct redistributor's completion status.
  Initially constrain the fixed QEMU redistributor mapping rather than claim
  physical GIC topology discovery.
- **Positive / negative regression:** enable/disable CPU2 PPI26 changes CPU2
  only; CPU0 state preserved. Invalid/uninitialized CPU or ID causes no MMIO.
  With reviewed service, two expiries/rearm and masked/future controls reproduce.
- **Unverified:** new API, disable/readback, redistributor discovery, remote
  configuration races, SGI policy and physical topology.

### T2 - Global timer policy lacks a CPU/service binding

- **Expected contract / source:** T; CNTHP is a per-PE compare/interrupt using
  PPI26. A partition deadline must identify the PE that owns and services it.
- **Observed / evidence:** `hv_timer_program_earliest_deadline` scans a global
  table and programs the calling CPU's registers, with no partition-to-CPU
  mapping. E `timer-detail/uart.log` has two compares 625,000 ticks ahead at
  62.5MHz and two IRQs; `timer-masked-detail/uart.log` has zero, CTL7;
  `timer-future/uart.log` has zero, CTL1. These use direct registers plus custom
  cancel/rearm/EOI, bypassing policy scheduling.
- **Cause / confidence:** confirmed source binding/service gap. No timer-policy
  cross-CPU interference or scheduler deadline failure was experimentally
  reproduced. Presence of API tables is not budget preemption evidence.
- **Dependencies:** A1, B3, T1, I2; I3 if expiry becomes a guest event.
- **Smallest correction:** define static partition/CPU timer ownership, reject
  unsupported remote programming, and select local deadlines only. Serialize
  local state and define expired/acknowledge/cancel behavior in the service path.
  Preserve source masking until it is installed. Review time conversion and
  overflow/rounding separately when choosing policy units.
- **Positive / negative regression:** independent CPU0/CPU2 deadlines, expiry,
  cancellation, acknowledgment and rearm with no cross-CPU register changes;
  masked/future controls; wrong CPU rejected before state/register mutation;
  already-expired/zero/overflow inputs follow a written contract; no IRQ storm.
- **Unverified:** policy-level delivery, multicore state safety, budget
  enforcement, conversion extremes, latency bounds and real-time guarantees.

## 4. Compiler alignment and runtime ABI

### A1 - Layout-sensitive EL2 alignment faults, enabling cause unresolved

- **Expected contract / source:** A. Compiler access assumptions must match
  effective EL2 memory attributes and alignment rules. Clearing SCTLR.A does
  not establish a complete memory contract. GCC's `-mstrict-align` constrains
  potentially unaligned generated accesses; it does not initialize translation
  or cache state.
- **Observed / evidence:** E `physical-spi/uart.log` and
  `physical-spi/fault-disassembly.txt`: unaligned `ldr x2,[x0]` at address
  `0x80005d8c`, ESR `0x96000021`, ELR `0x8000146c`, before IRQ assignment.
  `physical-spi-aligned/uart.log` and its disassembly show existing init's
  `stp x1,x1` at `0x80006fdc`, ESR `0x96000061`, ELR `0x800038ac`.
  Scalar probe assignments avoid the trigger in one layout.
- **Cause / confidence:** the faults and offending alignment are confirmed.
  Missing declared EL2 translation/cache handoff is source-confirmed.
  Effective Device-like attributes with stage-1 translation disabled are a
  plausible enabling explanation; decisive SCTLR/TCR/MAIR/TTBR readbacks were
  not captured. Do not promote that explanation to the sole proven cause.
- **Dependencies:** A2 unblocks the strict-build diagnostic. B2 memory handoff
  must consume the eventual decision. No IRQ correction is implicated by these
  failures occurring before the IRQ checkpoint.
- **Smallest correction:** first capture primary/secondary entry and post-init
  memory state under the two failing layouts. Choose a documented strict-access
  execution contract or an explicitly initialized Normal-memory translation
  regime after that evidence. Do not add EL2 page tables or compiler flags
  merely because one workaround booted.
- **Positive / negative regression:** preserve and reproduce the two failing
  images; controlled rebuilds reach the former failure checkpoints and retain
  intended guest denials. Check byte/word/aggregate accesses across meaningful
  alignments and initialized-data layouts. A deliberately invalid access still
  produces the expected diagnostic. Record cache and publication requirements.
- **Unverified:** exact effective attributes/state, complete cause, alternative
  compiler behavior, MMU transition correctness and runtime strict-build results.
  No production alignment fix is proposed in patch 1.

### A2 - Compiler-generated `memcpy` has no ABI definition

- **Expected contract / source:** GCC requires its freestanding runtime to
  supply standard memory functions. Source-level `#define memcpy hv_memcpy`
  cannot rename a libcall created during compilation. `hv_stage2_map_partition`
  contains aggregate assignments without that header. This finding concerns
  the specifically observed missing `memcpy` symbol.
- **Observed / evidence:** E
  `build-records/build-physical-spi-strict/build.log` fails at that reference.
  New V `build-baseline-strict.log` reproduces the same failure from pristine
  pinned source, without any probe/SMC/CPU-init change. V default and strict
  proposal build logs both link successfully; object symbols define `memcpy`,
  disassembly branches to the existing byte loop, and image undefined-symbol
  lists are empty. [memcpy_fixture.c](memcpy_fixture.c) passes native copy,
  return pointer, self-copy, zero-length and canary checks.
- **Cause / confidence:** confirmed missing link-time ABI symbol under the
  diagnostic flags; no conjectured IRQ or firmware dependency is needed.
- **Dependencies:** none of B/I/T/A1 to export and validate this symbol.
  This unblocks A1 investigation but does not resolve it.
- **Smallest correction:** ARM64-only standard `memcpy` wrapper in existing
  `string.c`, forwarding to `hv_memcpy`; retain production flags and public
  source aliases. See [proposal diff](0001-arm64-export-memcpy.patch).
- **Positive / negative regression:** pristine strict build fails on the exact
  symbol; identical proposal input links without libc; default build remains
  successful. Check ABI signature, symbol presence/host guard, helper call graph
  and copy behavior. Zero length preserves canaries, unsupported partial overlap
  is not claimed. Repeat required object checks when compiler/flags change.
- **Unverified:** ARM64 execution of this proposal, all optimization/toolchain
  combinations, complete GCC runtime ABI, and A1 runtime safety. Standard
  `memset`/`memmove`/`memcmp` exports remain separate work; exposing existing
  `hv_memmove` also merits review of relational comparisons between different
  C objects. No performance or timing claim follows from these build results.

## 5. Stage 1 additions (2026-10-31)

Added during Stage 1 at the workspace owner's direction. Entries above keep
their capture-time status.

### MM1 - Stage-2 mapping cannot express a permission restriction

- **Status:** source-derived; not executed. No code change.
- **Expected contract / source:** the comment on
  `arch/arm64/mm.c::hv_arch_stage2_map` at revision
  `f775304ce9443dc9861ba1880e4208c7ae19b314` documents `flags` as
  `S2AP_* | S2_MEMATTR_* | S2_SH_*`. `mm.c` is unmodified in the working tree.
  `struct hv_mem_region` carries an opaque `attrs`, which
  `hv_stage2_map_partition` passes through unchecked.
- **Observed / evidence:** the code does not decode those flags. `flags == 1`
  selects Device nGnRE RW, and every other value selects Normal WB RW. So no
  caller can obtain a read-only mapping, and an unrecognized value becomes
  cacheable RW. Current callers in `src/core/partition.c` pass 0 or 1.
- **Provenance:** found by source reading during
  [P-A0](../../stage1/20261031-pa0/README.md). The workspace owner reports
  confirmation against GitHub main by an outside read; this is recorded as
  reported and was not repeated in that session.
- **Cause / confidence:** the source-level mismatch is confirmed. Whether the
  defect is the code or the comment is undecided, because the intended semantics
  of `flags` are undecided.
- **Dependencies:** bears on any least-privilege claim and on P-A0 case F6.
- **Smallest correction:** semantics decided 2026-10-31 (owner approved
  Option A: portable `HV_MEM_R/W/X/DEVICE` bits, core validation, `attrs == 0`
  invalid). Design and acceptance tests:
  [MM1 proposal](../../stage1/20261031-mm1-semantics/PROPOSAL.md).
  Fix implemented 2026-10-31 in the working tree, uncommitted; host (L1)
  checks pass; ARM64 builds and links with the pinned GCC 15.2.0 on macOS
  (and GCC 11.4); no QEMU run yet: [MM1 BUGFIX](../../stage1/20261031-mm1-bugfix/README.md).
- **Positive / negative regression:** ARM64 QEMU negative test: map a page
  read-only, have the guest write to it, expect a stage-2 permission fault.
  Not yet possible to express, and not run.
- **Unverified:** runtime behaviour, and whether any caller relies on the
  current mapping of other flag values beyond those in `partition.c`.

### MM2 - Stage-2 execute-never value is wrong on CPUs with FEAT_XNX

- **Status:** source-derived; not executed. No code change. Recorded at the
  workspace owner's direction 2026-10-31.
- **Expected contract / source:** `arch/arm64/include/asm/page.h:80-86` at
  `f775304` defines `S2_XN_ALL` as bits 53 and 54 and documents it as "all
  execute never". `arch/arm64/mm.c:121` applies it to device mappings;
  `page.h:103,108` use it in `S2_PAGE_RO/RW`.
- **Observed / evidence:** in QEMU's implementation of ARMv8.2-TTS2UXN
  (FEAT_XNX), stage-2 bits 54:53 form `XN[1:0]`, where `11` means executable at
  EL1 and not at EL0; without FEAT_XNX, bit 53 is ignored and bit 54 means
  execute-never (https://patchwork.kernel.org/patch/11466337). So `S2_XN_ALL`
  is correct only on CPUs without FEAT_XNX.
- **Provenance:** found while writing the
  [MM1 proposal](../../stage1/20261031-mm1-semantics/PROPOSAL.md), section 2.
- **Cause / confidence:** the source reading is confirmed. The architecture
  semantics come from two independent secondary sources that agree: QEMU's
  TTS2UXN implementation and Linux KVM's `stage2_set_xn_attr` /
  `kvm_pgtable_stage2_pte_prot` (`arch/arm64/kvm/hyp/pgtable.c`, read
  2026-10-31; see the
  [QEMU readiness audit](../../stage1/20261031-mm1-qemu-readiness/AUDIT.md)).
  The Arm ARM table itself has not been read.
- **Dependencies:** any execute-never claim for device or data mappings;
  hidden on QEMU `-cpu cortex-a72` (no FEAT_XNX); CIX P1 support for FEAT_XNX
  unknown (`ID_AA64MMFR1_EL1.XNX`).
- **Smallest correction:** define execute-never as bit 54 only (`XN = 10`),
  rename the macros to the `XN[1:0]` field. Implemented in the MM1 BUGFIX
  2026-10-31 (uncommitted; L1 host test checks `XN = 10`).
- **Positive / negative regression:** QEMU instruction fetch from an XN page
  faults on both `-cpu cortex-a72` and `-cpu max`. Not yet expressible, not run.
- **Unverified:** the Arm ARM table; runtime behaviour on any CPU.

### EX1 - Logged IPA unreliable for stage-2 permission faults

- **Status:** source-derived + inference; not executed. No code change.
  Recorded at the workspace owner's direction 2026-10-31 (E1).
- **Expected contract / source:** `hv_handle_sync`
  (`src/core/exc/el2_exceptions.c`) derives the logged `IPA=` from
  `HPFAR_EL2` for every fault status code `0x04-0x0F`, including permission
  faults.
- **Observed / evidence:** Linux KVM (`__hpfar_valid` in
  `arch/arm64/kvm/hyp/include/hyp/fault.h`, read 2026-10-31) trusts
  `HPFAR_EL2` only for translation, access-flag and address-size faults, and
  for permission faults during a stage-1 walk; for a direct stage-2 permission
  fault it recomputes the IPA from `FAR_EL2` with `AT`.
- **Cause / confidence:** inference that HAVEN's logged IPA may be UNKNOWN for
  direct permission faults, which MM1 makes reachable for the first time.
  Confirm the `HPFAR_EL2` validity rule in the Arm ARM.
- **Dependencies:** MM1 L3 tests; any evidence that cites a logged IPA for a
  permission fault.
- **Smallest correction:** not proposed yet. Interim rule: judge permission
  faults by `ESR` (EC, FSC) and `FAR`, not the logged IPA.
- **Unverified:** runtime behaviour on QEMU and hardware; the Arm ARM rule.
- **Provenance:** [QEMU readiness audit](../../stage1/20261031-mm1-qemu-readiness/AUDIT.md) 2a.

### EX2 - Instruction-abort recovery repeats the fault

- **Status:** source-derived + inference; not executed. No code change.
  Recorded at the workspace owner's direction 2026-10-31 (E1).
- **Expected contract / source:** `hv_handle_sync` resumes the guest after any
  logged stage-2 fault by adding 4 to the saved ELR.
- **Observed / evidence:** for an Instruction Abort, ELR is the address of the
  instruction that could not be fetched, so ELR + 4 is the next instruction in
  the same execute-never page (architecture inference).
- **Cause / confidence:** inference: the guest faults once per instruction
  until it leaves the page (up to 1,024 log lines per 4 KB page), then runs
  whatever follows.
- **Dependencies:** MM2 L3 execute-never test.
- **Smallest correction:** owner decision E3 (2026-10-31): on a lower-EL
  instruction abort, stop the partition and log, instead of `ELR += 4`.
  Implemented 2026-10-31 (uncommitted) as `hv_s2_fault_action()` plus a
  parked CPU; host unit test passes; the stop path has not executed. See the
  [boot-profile BUGFIX](../../stage1/20261031-boot-profile-bugfix/README.md).
- **Unverified:** runtime behaviour; how "stop the partition" is realised in
  HAVEN's current control flow.
- **Provenance:** [QEMU readiness audit](../../stage1/20261031-mm1-qemu-readiness/AUDIT.md) 2b.

## 6. Stage 1 additions (2026-11-02): SMMU

Found while building SMMU read-back (A1, experiment G1). All three are latent
in the shipped configuration, because no production code path grants a DMA
device to a partition yet; they appear as soon as one does. No code change.

### SM1 - Stream IDs are allocated by table index, not bound to devices

- **Status:** source-derived + observed in QEMU (feasibility run S5,
  2026-11-02).
- **Expected contract / source:** granting a device to a partition programs the
  stream table entry of that device's hardware StreamID (for PCIe on QEMU
  `virt`, the requester ID through the `iommu-map` of the PCIe node).
- **Observed / evidence:** `hv_smmu_allocate_streamid()` returns the first free
  index of its own pool and `hv_smmu_configure_dma_window()` programs the STE at
  that index (`src/core/dma/smmu.c`). In run S5 the PCIe device with hardware
  SID 8 was granted to partition A: its STE stayed ABORT, and STE 0, which no
  device uses, was translated to partition A (`runs/v2b/S5-*`).
- **Consequence:** a grant cannot bind a specific device; the device stays
  blocked and an unused stream is opened instead.
- **Smallest correction (proposal):** take the device's hardware StreamID as an
  input of the grant and program exactly that STE.
- **Resolution:** fixed in haven `7ec513e` (2026-11-02), verified in QEMU
  with read-back v3 (`../../stage1/20261102-smmu-bugfix/README.md`, checks F1 and M2).

### SM2 - The DMA window is not enforced by hardware

- **Status:** source-derived + observed in QEMU (feasibility run S6).
- **Expected contract / source:** `hv_smmu_configure_dma_window()` documents a
  DMA window (`dma_base`, `dma_size`, `phys_base`) per device.
- **Observed / evidence:** the window is stored and checked only in software
  (`hv_smmu_check_dma_access`); the STE points the device at the partition's
  whole stage-2 table (`STE.S2TTB` = the partition's VTTBR). In run S6 a
  device granted a 1 MiB window could reach `0x20201000` bytes, all of
  partition A.
- **Consequence:** a device can DMA anywhere its partition can, not only into
  its window; the grant is narrower than the enforcement.
- **Smallest correction (proposal):** give each device stream its own stage-2
  table limited to the window, or record that HAVEN grants DMA at partition
  granularity and drop the window from the contract.
- **Resolution:** fixed in haven `7ec513e` (2026-11-02), verified in QEMU
  with read-back v3 (`../../stage1/20261102-smmu-bugfix/README.md`, checks F1, F2, M1, M3 and M4).

### SM3 - The SMMU command queue is never set up

- **Status:** source-derived, register offsets checked against Linux
  `drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h`; observed as a hang in QEMU
  (runs S5 and S6: the boot log stops after "partition A mapped").
- **Expected contract / source:** SMMUv3 commands (`CFGI_STE`, `CMD_SYNC`) are
  written to a queue in memory whose address is in `SMMU_CMDQ_BASE` (0x90),
  with `SMMU_CMDQ_PROD` at 0x98, `SMMU_CMDQ_CONS` at 0x9c and the queue enabled
  by `SMMU_CR0.CMDQEN` (bit 3) (Linux: `ARM_SMMU_CMDQ_BASE 0x90`,
  `ARM_SMMU_CMDQ_PROD 0x98`, `ARM_SMMU_CMDQ_CONS 0x9c`, `CR0_CMDQEN (1 << 3)`).
- **Observed / evidence:** `arch/arm64/include/asm/smmu.h` defines
  `SMMU_CMDQ_PROD 0x8c` and `SMMU_CMDQ_CONS 0x90`; `smmu_v3.c` writes commands
  into the register page at `SMMU_CMDQ_BASE + index * 16`; `smmu_v3_init()`
  never programs `SMMU_CMDQ_BASE` and never sets `CMDQEN`. The first STE change
  after boot waits for a consumer index that never moves, then halts (`b .`).
- **Consequence:** any runtime grant or revocation hangs HAVEN; without working
  invalidation, the SMMU could also keep using a cached STE after a change.
- **Smallest correction (proposal):** allocate an aligned command queue in
  memory, program `SMMU_CMDQ_BASE`, fix the PROD/CONS offsets, enable `CMDQEN`
  and wait for `CR0ACK`, then keep the existing CFGI_STE + CMD_SYNC sequence.
- **Note for read-back:** read-back reads the stream table in memory; once
  invalidation works, the SMMU's cached view and the table agree. Until then
  the two can differ.
- **Resolution:** fixed in haven `7ec513e` (2026-11-02), verified in QEMU
  with read-back v3 (`../../stage1/20261102-smmu-bugfix/README.md`, QEMU command trace and checks F1, F3).

## 7. Stage 2 additions (2026-11-02): GIC

Found while building GIC read-back (`../../stage2/20261102-gic-readback/`).
Both fixed on 2026-11-02 after owner approval
(`../../stage2/20261102-irq-delivery/README.md`).

### GI1 - GICD_CTLR written with the Secure-view bit layout

- **Status:** source-derived (Linux `include/linux/irqchip/arm-gic-v3.h`)
  + observed in QEMU (GICD_CTLR reads `0x53` after HAVEN's write).
- **Expected contract / source:** HAVEN runs at Non-secure EL2. A
  Non-secure access to GICD_CTLR sees ARE_NS at bit 4, EnableGrp1A at bit 1
  and EnableGrp1 at bit 0 (Linux: `GICD_CTLR_ARE_NS (1U << 4)`,
  `GICD_CTLR_ENABLE_G1A (1U << 1)`, `GICD_CTLR_ENABLE_G1 (1U << 0)`). With
  a single security state (DS = 1) ARE is bit 4 and EnableGrp1 bit 1.
- **Observed / evidence:** `arch/arm64/include/asm/gic.h` defines
  `GICD_CTLR_ARE_NS (1U << 5)`, `GICD_CTLR_ENABLE_G1NS (1U << 1)`,
  `GICD_CTLR_ENABLE_G0 (1U << 0)` (the Secure view), and `gicd_init()`
  writes 0, then those bits. In QEMU (DS = 1, ARE read-as-one) the result
  `0x53` happens to be correct.
- **Consequence (inference):** on a board with two security states (the
  CIX P1 boots through Secure firmware) the write of 0 clears the
  Non-secure group enable and may try to clear ARE_NS, and the second write
  sets a reserved bit instead of ARE_NS. Affinity routing then depends on
  what firmware left behind.
- **Smallest correction (proposal):** use the Non-secure layout (bit 4
  ARE_NS, bit 1 EnableGrp1A), read DS to choose the single-security layout,
  and never write ARE as 0 once set: read-modify-write, waiting on RWP.
- **Resolution:** fixed in `de6291c` with the Linux sequence (write 0, wait
  RWP, then ARE_NS | EnableGrp1A | EnableGrp1); the same bit positions are
  correct for DS = 1, so no DS branch is needed. Not demonstrable in QEMU
  `qemu-direct` (single security state); to be confirmed on the board.

### GI2 - Interrupts granted to a partition are never delivered

- **Status:** source-derived + observed (GIC read-back: SPI 33 enabled and
  routed to CPU 0; no EL2 interrupt path before `1f84fe0`).
- **Observed / evidence:** HAVEN routes and enables owned SPIs in the
  distributor, but until `1f84fe0` it never enabled Group 1 at its CPU
  interface and had no handler; it has no virtual injection of physical
  interrupts into partitions.
- **Consequence:** an interrupt grant is enforced (it cannot reach another
  partition) but useless to its owner; partitions must poll.
- **Status after `1f84fe0`:** HAVEN takes physical interrupts at EL2 and
  handles its own (SMMU events); interrupts owned by partitions are
  completed without delivery. Virtual injection through the list registers
  is still to do.
- **Resolution:** `f6fe485` injects an interrupt into its owner, if that
  partition runs on the CPU, as a hardware-linked virtual interrupt;
  `3fbafae` disables an SPI that cannot be delivered. Partition A receives
  its UART interrupt (checks J1-J4). Open: physical interrupts on CPUs
  other than the boot CPU.

## 8. Stage 2 additions (2026-11-02): SMMU, from case set v3

### SM4 - A DMA window's VMID leaks into its stage-2 table address

- **Status:** resolved in haven `2b6f5ea` (owner approval 2026-11-02).
  Observed in QEMU (case set v3, S9) + source-derived.
- **Expected contract / source:** STE.S2TTB holds the table base, bits
  [51:4] (Linux `STRTAB_STE_3_S2TTB_MASK GENMASK_ULL(51, 4)`); the VMID goes
  in STE.S2VMID (word 2).
- **Observed / evidence:** `smmu_v3_set_ste_partition()` receives a
  VTTBR-format value (`vttbr_encode`: VMID in bits [63:48]) and writes
  `ste[3] = vttbr & STE_3_S2TTB_MASK`. With VMID 1 (S9) S2TTB read back as
  `0x1000080010000` and the SMMU reported 4 "bad STE" events; DMA failed.
- **Consequence:** any DMA VMID with bits [3:0] non-zero corrupts the table
  address. HAVEN allocates `0xC0`, `0xC1`, ...: only `0xC0`, `0xD0`, `0xE0`
  and `0xF0` are safe, so the first window works and the second (`0xC1`)
  would already be rejected.
- **Smallest correction (proposal):** pass the table base, not a VTTBR value,
  to the driver (or clear the VMID with `VTTBR_BADDR_MASK` before applying
  `STE_3_S2TTB_MASK`), and add a unit-level check that S2TTB equals the
  table base for VMIDs `0xC0` to `0xFF`.
- **Resolution:** `arch/arm64/include/asm/smmu_ste.h` adds
  `smmu_ste3_s2ttb()`, which clears VTTBR bits [63:48] before applying the
  S2TTB mask; `smmu_v3_set_ste_partition()` uses it. Host test
  `test_smmu_ste` checks S2TTB equals the table base for VMIDs `0xC0` to
  `0xFF`, VMID 1 and CnP. S9 rerun at `ce220bb` (case set v3,
  `runs-after-sm4/`): S2TTB `0x80010000`, no "bad STE" event, DMA inside
  the window succeeds.

### RB1 - Read-back crashes when HAVEN halts before stage-2 setup

- **Status:** observed in QEMU (held-out case R3, 2026-11-02). Tool defect,
  not a HAVEN defect. No change yet: `readback.py` v3 is frozen, so the fix
  goes into a new tool version with its own freeze.
- **Observed / evidence:** with the SMMU init fault R3, HAVEN panics before
  filling `pt_roots`; read-back walks root `0x0` and raises `ValueError: PA
  0x0 outside the dumped image` (exit status 1). The v3 runner maps any
  exit status 1 to "findings", so its summary table reports findings that
  do not exist.
- **Smallest correction (proposal):** treat a zero root as a probe result
  ("HAVEN halted before stage-2 setup", exit 2) and make the runner require
  a report file before counting findings.

### RB2 - Read-back does not flag a granted PPI that is disabled

- **Status:** observed in QEMU (partition B control K3, 2026-11-02). Tool
  gap, not a HAVEN defect; for a future read-back version.
- **Observed / evidence:** with PPI 27 on CPU 2 given to partition A in
  HAVEN's table, HAVEN disables it after refusing delivery; read-back
  against the grant (PPI 27 on CPU 2 for B) reports no finding.
  `compare_gic` checks SPIs both ways (granted but disabled, enabled
  without grant) but PPIs only one way.
- **Smallest correction (proposal):** report "missing interrupt binding"
  for each granted PPI not enabled on its CPU, as for SPIs.
