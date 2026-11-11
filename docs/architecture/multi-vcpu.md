# Multi-vCPU partitions: design (committed work since G2)

**Status:** design, owner-approved to start 2026-11-07 (G2 promoted
multi-vCPU partitions to committed work, Linux SMP first). SMP0 to SMP3
implemented 2026-11-08 (haven `88cd9e0`, `a27dc96`, `1af894b`, `dab23d5`); where the code
differs from this design, section 7 says so. Labels: **[F]** fact from our
code or records, **[I]** inference, **[P]** proposal. Expectations and
results: `../methodology/stage2/20261108-multi-vcpu/README.md`.

## 1. Why

The CIX P1 layout accepted at G2 gives Linux 8 cores and the real-time
partition 2 to 4 (`cix-p1-partition-plan.md`). HAVEN runs one vCPU per
partition and answers PSCI CPU_ON with DENIED [F, `src/core/exc/psci.c`].
The first target is partition A (Linux) on two CPUs in QEMU: CPUs 0 and
1, with partition B on CPU 2, as today.

## 2. Principle: static pinning, no scheduler

Each vCPU of a partition is pinned to one physical CPU listed in its
configuration; a physical CPU serves at most one vCPU. HAVEN never
migrates or time-shares vCPUs. This keeps static partitioning's property
that a CPU belongs to one partition, and keeps read-back simple: HAVEN's
per-CPU partition table (`cpu_partition`, read-back v10) stays the truth
about who runs where [P].

## 3. Pieces

| Piece | Today [F] | Design [P] |
| - | - | - |
| Configuration | partition A: CPU 0 | `part_a_cpus = {0, 1}` in `partition.c`; the guest's device tree lists both, `enable-method = "psci"`, and two redistributor frames |
| PSCI CPU_ON | DENIED | allowed only for a CPU in the calling partition's list that is off: HAVEN powers it on through the firmware's PSCI (as for partition B), the secondary sets up EL2 (same VTTBR and VMID as the boot vCPU, HCR, its own redistributor model), records `cpu_partition[cpu] = A`, and enters the guest at the requested entry with `x0` = context ID. Anything else stays DENIED |
| AFFINITY_INFO, CPU_OFF | one vCPU | per-vCPU state ON / OFF / ON_PENDING; CPU_OFF parks the CPU at EL2 and clears its table entry |
| SYSTEM_OFF / RESET | stops the calling CPU | stops every vCPU of the partition: HAVEN sends its own SGI to the other CPUs of the partition, which park |
| Emulated GIC | one model per CPU holding distributor and redistributor state together (`struct hv_vgic`) | split: one **distributor model per partition** (GICD_CTLR, SPI enable, group, priority, IROUTER) shared by its vCPUs under a lock, one **redistributor model per vCPU** (SGIs, PPIs, waker, affinity) |
| SPI routing | SPI pinned to the granted CPU | unchanged: SPIs stay on the CPU the grant names; a guest write to IROUTER is kept in the model but does not move the hardware route (Linux's affinity changes then have no effect; recorded as a limit) |
| SGIs between vCPUs | only self-targeted SGIs are injected; others dropped | the trapped ICC_SGI1R write is decoded (target list and affinity, or IRM = all others); for a target on another CPU HAVEN sets a pending bit in that vCPU's mailbox and sends **one HAVEN-reserved physical SGI** to its CPU, whose EL2 handler injects the virtual SGIs |
| Timers | virtual timer PPI 27 owned by A on CPU 0; HAVEN's budget timer PPI 26 | PPI 27 owned by A on CPUs 0 and 1 (enabled when that vCPU's guest enables it); budget per CPU as today |
| Stage-2 | one table set per partition | unchanged; both CPUs load the same VTTBR. The tables do not change after boot, so no cross-CPU TLB invalidation is needed [I] |
| HAVEN's EL2 stacks and per-CPU data | exist for CPUs 0 and 2 | used for CPU 1 too |

## 4. Read-back (v12) [P]

- Grant: `gic.cpus` gives CPU 1 to partition 1; `ppis` and
  `guest_enabled` get CPU 1's PPI 27; `haven_ppis` CPU 1's PPI 26.
- New: `gic.cpus_on_request`, the CPUs HAVEN starts only when the guest
  calls CPU_ON. Before that, "CPU n runs no partition" is reported as
  **waiting for the guest** (information), as v8 does for interrupts the
  guest has not enabled yet; after it, the v10 rule applies.
- The emulated GIC's enable bits move (distributor shared, redistributor
  per vCPU): read-back reads both through new symbols, with compile-time
  offset checks as for `hv_vgic.enabled` today.
- Faults to seed later: vCPU 1 started on the wrong physical CPU (CPU
  assignment drift on two CPUs); an SGI injected into the wrong vCPU (no
  state left behind: read-back cannot see it, unit tests must).

## 5. Trusted code

Expected growth [I]: PSCI CPU_ON and per-vCPU state about 80 lines, the
vGIC split about 80, cross-CPU SGIs about 60, configuration and secondary
entry about 40: roughly 250 code lines over the 6,719 measured after L4
(`../methodology/stage2/20261104-linux-partition/TCB-L1-L3.md`). Measured
with `tools/scripts/tcb-image-loc.py` after the change, reported
separately from read-back as before.

## 6. Steps

1. **SMP0:** unit tests first (PSCI CPU_ON rules, SGI target decoding,
   shared distributor), then the code.
2. **SMP1:** Linux on two vCPUs in QEMU (`qemu-psci`, partition B on CPU
   2 beside it).
3. **SMP2:** read-back v12 and seeded faults.
4. Later: more vCPUs, then the board layout (8 for Linux, 2 to 4 for the
   real-time partition), which also needs the board's CPU map. **Done
   for partition A** (2026-11-12, `../methodology/stage2/20261112-multi-vcpu-n/`):
   N vCPUs pinned by the platform (`PART_A_CPU(i)`), 8 on cix-p1
   (compile-only), up to 3 on qemu-virt (Linux boots on 3 in QEMU). The
   real-time partition waits for its own record.

## 7. As implemented (SMP0 to SMP3) [F]

Built only with `-DHAVEN_PART_A_VCPUS=2` and a Linux guest; the default
(1) keeps the one-vCPU code paths. Differences from sections 3 to 5:

- **Emulated GIC not split.** `struct hv_vgic` is unchanged (read-back's
  offset check still holds). Each vCPU has its own model; the
  distributor region of both CPUs is served by vCPU 0's model, so SPI
  state lives there once, and each vCPU's redistributor frame by its own
  model. Both CPUs map both frames (Linux scans them for its affinity);
  GICR_TYPER.Last is set on the last frame only. One spinlock serialises
  all accesses to these models. Read-back needs no new symbols for this.
- **CPU_OFF** (since SMP3) marks the vCPU OFF, sets `cpu_partition` to
  none, resets the CPU's state, mailbox and request words and switches it
  off through the firmware's PSCI CPU_OFF; a later CPU_ON starts it again
  by the first path. (SMP1 parked the CPU and refused a second CPU_ON.)
- **Pinning** (since SMP2): `PART_A_CPU1` is vCPU 1's physical CPU; its
  guest MPIDR comes from the vCPU table and is written to VMPIDR_EL2, so
  the two can differ.
- **CPU_ON** also checks that the entry address lies in partition A's
  RAM (INVALID_ADDRESS otherwise).
- **Requests between CPUs** use HAVEN's physical SGI 1 and a per-CPU
  request word (bits 0 to 15: virtual SGIs; bit 31: stop). An SGI to a
  vCPU that has it disabled is kept pending in that vCPU's model and
  delivered once when it is enabled (since SMP3; SMP0 to SMP2 dropped it).
  One that finds no free list register waits for the next physical
  interrupt on that CPU. The one-vCPU path still drops a disabled
  self-SGI.
- **Trusted code:** +425 code lines for SMP0 and SMP1 (6,762 to 7,187,
  estimate 250), +29 for SMP2, +126 for SMP3 (estimate 40): 7,342 in all.
