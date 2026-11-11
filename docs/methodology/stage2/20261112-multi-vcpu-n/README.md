# N vCPUs for partition A (multi-vCPU step 4)

**Status:** owner-approved step (2026-11-11), step 4 of
`../../../architecture/multi-vcpu.md`. Design and expectations by Claude,
merged before any code; results added by a later pull request and
expectations never edited after them. Labels: **[F]** fact from our code
or records, **[I]** inference, **[P]** proposal. No board: the cix-p1
build is compile-only; QEMU exercises the generalised path.

## Problem

[F] Partition A supports 1 or 2 vCPUs (`HAVEN_PART_A_VCPUS`): the code
names vCPU 1 (`part_a_cpu = {0, PART_A_CPU1}`, `part_a_peers` with two
entries, a vCPU-1 block in `part_a_vgic_setup`, one PPI 27 route for
vCPU 1), `HV_PSCI_MAX_VCPUS` is 4 and `HV_MMIO_MAX_REGIONS` 8. The
cix-p1 layout gives partition A logical CPUs 0 to 7 (owner, D3), which
needs 8 vCPUs: 8 entries in the PSCI vCPU table and 8 x (1 + 8) = 72
emulated regions (each CPU registers the distributor and all 8
redistributor frames, as SMP1 does for 2).

## Design

- **Pinning from the platform.** `memory.h` defines `PART_A_MAX_VCPUS`
  and `PART_A_CPU(i)`, the physical CPU of vCPU i:
  - cix-p1: 8, `PART_A_CPU(i) = i` (owner: vCPU i on logical CPU i);
  - qemu-virt: 3, vCPU 0 on CPU 0, vCPU 1 on `PART_A_CPU1` (the existing
    knob, default 1), vCPU 2 on CPU 3, because CPU 2 is partition B's and
    QEMU's CPU map has 4 CPUs. **4 vCPUs on qemu-virt are not possible**
    without moving partition B or growing qemu-virt's CPU map, which
    would change the default images; 3 is the largest QEMU check of
    N > 2.
- **Guest MPIDR** of vCPU i is logical CPU i's MPIDR
  (`hv_arch_logical_mpidr(i)`), as for vCPU 1 today: on cix-p1 this is
  the physical MPIDR of the CPU it runs on (D3 decision); on qemu-virt
  vCPU 2 shows MPIDR 2 while it runs on CPU 3 (pinning and guest identity
  may differ, `multi-vcpu.md` section 7).
- **Generalised code** in `partition.c`: `part_a_cpu[]` goes (callers use
  `PART_A_CPU(i)`); `part_a_peers[]` is filled at setup; vCPUs 1 to N-1
  are set up in one loop (model, GICR_TYPER.Last only on the last frame,
  vCPU table entry OFF); their PPI 27 routes are assigned in a loop after
  the static table (vCPU 0's route stays in the table). The log says
  `partition A has N vCPUs` once and one line per waiting vCPU.
- **Capacity per platform:** `HV_PSCI_MAX_VCPUS` and `HV_MMIO_MAX_REGIONS`
  become overridable (`HAVEN_PSCI_MAX_VCPUS`, `HAVEN_MMIO_MAX_REGIONS`,
  as `HAVEN_VGIC_NR_IRQS`). CMake: cix-p1 always 8 and 72 (owner); a
  qemu-virt build with more than 2 vCPUs gets N x (N + 1) regions (12 for
  3), with the default 4-entry PSCI table. Static assertions: N <=
  `HV_PSCI_MAX_VCPUS`, N x (N + 1) <= `HV_MMIO_MAX_REGIONS`, N <=
  `PART_A_MAX_VCPUS`. Read-back already takes the table size from the
  image (D5 step 3).
- **Unchanged:** one spinlock over all models, SPIs pinned to the granted
  CPU, SGIs through peers (bit masks of 32), static pinning, CPU_ON only
  for a CPU of the partition that is off.
- **Host test:** a third build of `test_partition_layout`, cix-p1 with 8
  vCPUs: routes SPI 330 on CPU 0 and PPI 27 on CPUs 0 to 7, in order.
- **QEMU:** `run_n.py` in this folder (reuses `run_smp1.py`'s build, tree
  and boot): Linux v6.18 and KDAL as SMP1's F1, partition A with N = 2
  and N = 3 vCPUs, `qemu-psci`, `-smp 4`; the tree gets N cpu nodes and N
  redistributor frames. Read-back runs are not repeated.
- **CI:** the cix-p1 Linux step builds with 8 vCPUs.

## Expectations

Baseline: `main` at `2219898`, Homebrew `aarch64-linux-gnu-gcc` GCC
15.2.0 on the macOS host, images built with `SOURCE_DATE_EPOCH=1793318400`.
Variants as D5 (`../20261109-d5-platform-layout/`), plus qemu-linux3
(qemu-linux with `-DHAVEN_PART_A_VCPUS=3`) and cix-linux8 (`arm64-cix-p1
-DHAVEN_PART_A_DTB_IPA=0x110000000 -DHAVEN_PART_A_VCPUS=8`). Baseline
text / data / bss: qemu and psci 43,148 / 144 / 5,251,064; qemu-linux
45,724 / 144 / 5,252,496; cix 41,720 / 144 / 5,251,064; cix-linux (2
vCPUs) 44,320 / 144 / 5,258,048. Trusted code: qemu 66 files, 7,522
lines; cix 66, 7,511. CTest 29, Python 59. [F] `run_smp1.py` F1 on the
baseline (checked before this record): Linux brings up 2 CPUs, KDAL
reaches `KDAL-DEMAND-END`, both vCPUs stop, partition B starts on CPU 2.

"+ padding" allows 0 to 4,095 bytes before a page-aligned object.

| Row | Check | Expected result |
| - | - | - |
| M1 | qemu, psci | `haven.bin` byte-identical to the baseline's |
| M2 | qemu-linux (2 vCPUs) | builds; not byte-identical (generalised code); bss 5,252,512 + padding (`part_a_peers`, 16 bytes, moves from read-only data to bss) |
| M3 | qemu-linux3 | builds; bss = M2's + 848 (+ padding): one model 664, entry and context words 16, peers 8, 4 more MMIO regions 160 |
| M4 | cix | builds; bss 5,253,624 + padding (72 MMIO regions, +2,560) |
| M5 | cix-linux (2 vCPUs) | builds; bss 5,260,672 + padding (+2,560 MMIO, +48 PSCI table of 8, +16 peers) |
| M6 | cix-linux8 | builds, no compiler warning; bss 5,281,456 + padding (M5 + 6 models 20,640 + entry and context 96 + peers 48) |
| M7 | qemu with `-DHAVEN_PART_A_VCPUS=4`; cix with 9 | both refused at compile time by the `PART_A_MAX_VCPUS` check |
| M8 | QEMU F1, N = 2 (`run_n.py`) | Linux `smp: Brought up 1 node, 2 CPUs`; `KDAL-DEMAND-END`; `CPU 1 ready, partition A vCPU 1 started`; stops: `CPU 1 parked` (by its vCPU on another CPU) and `partition stopped by PSCI OFF`; `partition B started`; no `not delivered` line |
| M9 | QEMU F1, N = 3 | Linux `smp: Brought up 1 node, 3 CPUs` with `CPU2: Booted secondary processor 0x0000000002`; `KDAL-DEMAND-END`; `CPU 3 ready, partition A vCPU 2 started`; stops: CPUs 1 and 3 parked, then `partition stopped by PSCI OFF`; `partition B started` (CPU 2); no `not delivered` line |
| M10 | host CTest | 30 of 30 (`test_partition_layout_cix8` new) |
| M11 | Python tests | 59, all pass |
| M12 | trusted code | qemu 66 files, 7,525 to 7,560 code lines; cix 66 files, 7,515 to 7,550 |
| M13 | CI on the code pull request | every job green, the cix-p1 Linux step with 8 vCPUs |

Verdict rule as D5. Not shown by this step: 8 vCPUs running (no board),
Linux on more than 3 vCPUs, read-back of an N-vCPU partition, or any
performance property of 72 linearly searched MMIO regions.

## Results (2026-11-12, macOS host and CI)

Expectations merged first (haven `fd53ab1`, pull request 74); the code
after that (commit `483ee2e`, merged as `2b4b697`, pull request 75, CI
green on every job). Images built with `SOURCE_DATE_EPOCH=1793318400`,
"after" from the code commit's tree. QEMU runs: `python3 run_n.py
<workspace>/haven <workspace>/kdal <Linux v6.18 Image> kdal.ko runs` on
`main` at `2b4b697` (logs in `runs/`; the same rows were also seen on a
scratch commit of the same tree before the pull request).

| Row | Observed | Verdict |
| - | - | - |
| M1 | qemu `4fb97f52...`, psci `a58b6143...`: equal to the baseline (`cmp`) | as expected |
| M2 | builds; differs from the baseline; 45,736 / 144 / **5,252,512** | as expected |
| M3 | builds; 46,004 / 144 / **5,253,360** (M2 + 848, no padding) | as expected |
| M4 | builds; 41,720 / 144 / **5,253,624** | as expected |
| M5 | builds; 44,256 / 144 / **5,260,672** | as expected |
| M6 | builds, no compiler warning (only the known linker lines); 44,424 / 144 / **5,281,456** | as expected |
| M7 | both stop with `#error "HAVEN_PART_A_VCPUS must be from 1 to the platform's PART_A_MAX_VCPUS"` | as expected |
| M8 | Linux `smp: Brought up 1 node, 2 CPUs`; `KDAL-DEMAND-END`; `CPU 1 ready, partition A vCPU 1 started at IPA 0x41e40570`; `CPU 1 parked` (by its vCPU on another CPU), then `partition stopped by PSCI OFF`; `CPU 2 ready, partition B started`; no `not delivered` | as expected |
| M9 | Linux `CPU2: Booted secondary processor 0x0000000002`, `smp: Brought up 1 node, 3 CPUs`; `KDAL-DEMAND-END`; `CPU 3 ready, partition A vCPU 2 started at IPA 0x41e40570`; CPUs 1 and 3 parked, then `partition stopped by PSCI OFF`; `CPU 2 ready, partition B started`; virtual interrupts delivered on CPUs 0, 1 and 3; no `not delivered` | as expected |
| M10 | 30 of 30 locally and in CI (Linux, macOS, Windows); `scripts/dev/test.sh` passes | as expected |
| M11 | 59, OK | as expected |
| M12 | qemu 66 files, 7,543; cix 66 files, 7,531 | as expected |
| M13 | pull request 75: every job green, `Cross-compile (cix-p1 draft, Linux layout, no boot)` with 8 vCPUs | as expected |

**Verdict.** 13 of 13 rows hold as pre-registered; every bss figure was
exact (no padding). Linux runs on 3 vCPUs of partition A in QEMU with
vCPU 2 pinned to CPU 3 and seeing MPIDR 2, beside partition B on CPU 2,
through the same loops the cix-p1 build uses for 8.

What this does not show [I]: 8 vCPUs running (compile-only, no board),
Linux on more than 3 vCPUs, read-back of an N-vCPU partition (not run),
or the cost of 72 linearly searched MMIO regions and one lock over 8
models on the trap path (to measure on the board).

Next [P]: partition B's own record on cix-p1 (memory, CPUs 8 to 11,
whether it gets several vCPUs); read-back on QEMU with 3 vCPUs (grant
with `cpus_on_request` 1 and 3) when read-back is next exercised.
