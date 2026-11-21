# Read-back v15: each CPU's stage-2 register, not HAVEN's table (design)

**Correction (2026-11-21):** the probe was already at v14 (2026-11-12, D5
step 3), so this change is **v15**; the rows keep their names. #165 and
#167 call it v14.

**Status:** design and expectations, 2026-11-21, written before any code
(owner-approved item: "the W09 fix designed with expectations first").
**[F]** fact, **[I]** inference, **[P]** proposal.

## Problem [F]

Campaign case W09 (`../20261119-evaluation-campaign/c2/RESULTS-C2.md`)
makes CPU 2 enable stage 2 with partition A's tables
(`hv_arch_stage2_enable(PARTITION_A_ID, PARTITION_B_ID)`) while HAVEN's
own table, `cpu_partition`, still says partition B. Read-back passed it;
only partition B's own check (M4) saw the fault. Read-back's CPU check
(v10, `tools/readback/readback.py`, `compare_cpus`) compares the grant's
`gic.cpus` with `cpu_partition`: HAVEN's bookkeeping, not the register
the hardware uses. A fault between the two is invisible, which is the
blind spot read-back exists to close for every other enforcement point
(it reads stage-2 tables, STEs and GIC registers, not HAVEN's intent).

## What enforces the CPU's partition [F]

On ARMv8 with EL2, a CPU's guest accesses go through the stage-2 tables
whose base address and VMID are in its `VTTBR_EL2` (BADDR, bits 47:1;
VMID, bits 63:48 or 55:48), with stage 2 on when `HCR_EL2.VM` is set.
Read-back already knows every partition's stage-2 root (`pt_roots`).

## Change [P]

1. **Smallest experiment first (V0).** Start QEMU with its gdb stub, as
   read-back's v9 `--break` mode does, ask for the target description
   (`qXfer:features:read`) and check that it names `VTTBR_EL2` and
   `HCR_EL2` for every vCPU, on QEMU 8.2.2, 10.0 (the campaign's
   container) and the Mac's 11.1. If it does not, this design stops and
   the record says so; the fallback (HAVEN storing the value it wrote,
   which is bookkeeping again) is not taken without the owner.
2. **Read-back v14.** The gdb stub is always attached, the machine paused
   after the memory snapshot; for each vCPU (`Hg<thread>`), `VTTBR_EL2`
   and `HCR_EL2` are read. Per CPU:
   - granted partition p (grant `gic.cpus`), `HCR_EL2.VM` set and BADDR
     equal to p's root: as granted;
   - BADDR equal to another partition's root: **"CPU stage-2 drift"**,
     naming both partitions;
   - BADDR that is no partition's root, or VM clear while a partition is
     granted: **"CPU stage-2 drift"** with the value;
   - a CPU granted to no partition, or a `cpus_on_request` CPU still
     waiting: no finding while VM is clear (information otherwise).
   The v10 table check stays as it is (both are reported).
3. Unit tests for the comparison with synthetic register values.

## Expectations

| Row | Check | Expected |
| - | - | - |
| V0 | the gdb stub's target description on QEMU 8.2.2, 10.0 and 11.1 | names `VTTBR_EL2` and `HCR_EL2`; reading them on a paused vCPU returns 64-bit values |
| V1 | unit tests | each of the four cases above gives its verdict; VMID width (8 or 16 bits) does not change BADDR |
| V2 | read-back v14 on the campaign baseline and the five C2 controls, both CPU models | no CPU stage-2 drift; every other verdict unchanged |
| V3 | W09 again (its frozen patch) | M6: `CPU 2: CPU stage-2 drift` (runs partition 1's tables, granted partition 2) on both CPU models |
| V4 | the other 43 C2 cases (written and corrected) | the same verdicts as their recorded runs |
| V5 | M6 run time | median grows by under 1 s |

V2 to V5 run through `campaign.yml` with the frozen C2 cases files; a
new freeze (`SHA256SUMS-V14`) covers the changed read-back before the run.
W09's row in `RESULTS-C2.md` stays as it is: the blind spot is the
result of read-back v13; v14's result is recorded beside it.

## Result of V0 (2026-11-21) [F]

`tools/readback/gdb_sysreg_probe.py` (#166): QEMU started paused with
`virtualization=on`, `-cpu max`, two vCPUs; target description read,
then `VTTBR_EL2` and `HCR_EL2` on each vCPU (`qfThreadInfo`, `Hg`, `p`).

| QEMU | Where | Feature files | VTTBR_EL2 | HCR_EL2 | Read on both vCPUs |
| - | - | - | - | - | - |
| 8.2.2 | Ubuntu 24.04 runner (workflow `readback-probe.yml`) | core, SVE, pauth, `system-registers.xml` | regnum 128 | regnum 200 | yes, 64-bit, 0 at reset |
| 10.0.13 | `debian:trixie` container (the campaign's) | the same | regnum 127 | regnum 203 | yes |
| 11.1.3 | the Mac (Homebrew) | also SME, SME2, TLS | regnum 144 | regnum 222 | yes |

**V0 holds.** The register numbers differ between versions, so v14 finds
both registers by name in the target description, never by number.

## Before V2 to V5: W09 may not do what this design assumed (2026-11-21)

V1 holds (`tools/readback/test_cpu_stage2.py`, 8 tests). A first run of
v15 on the Mac (QEMU 11.1.3, `qemu-psci` build, both CPU models) [F]:

- the unchanged tree: CPU 0 `VTTBR_EL2 0x100008000e000` (VMID 1,
  partition 1's root), CPU 2 `0x2000080012000` (VMID 2, partition 2's),
  CPUs 1 and 3 stage 2 off; no finding (V2's form);
- **W09**: CPU 2 `0x2000080012000`, partition 2's tables; no finding;
  and partition B's own check (M4) **passes** on both CPU models.

Why [I, from the code]: W09 changes the VTTBR_EL2 written at secondary
init (`partition_b_secondary_cpu_init`), but the world switch
(`arch/arm64/context.S`, `msr vttbr_el2, x2`) loads VTTBR_EL2 from the
vCPU's saved context before partition B runs, so the wrong value lives
only between the two writes, with no guest access in between. If so, W09
is an equivalent mutant (design item 2: counted apart), and its C2 row,
M4 a finding under QEMU 10.0, needs re-checking: a flaky M4 there is the
other explanation (C7 had one unexplained M4 failure on `max` in C1's
third run).

**Added row, before any further run:**

| Row | Check | Expected |
| - | - | - |
| V6 | W09 five times in CI (`tools/campaign/cases/w09-repeat.json`, v15) | if W09 is equivalent: every method passes in all five; if M4 fails in some and passes in others, M4 is flaky in CI and that is recorded as its own finding |

V3 stands as written until V6 has run; if V6 shows W09 is equivalent, V3
is recorded as a prediction error (the design misread the case), and
v15's CPU check is tested instead by a case whose wrong VTTBR_EL2 lasts
(a change in the vCPU context, written as its own case with the owner's
approval, like any C2 case).

## Results (2026-11-21)

All runs: `campaign.yml` at `0c6d755` (the merge of #170), QEMU 10.0.13
in `debian:trixie`; `SHA256SUMS-V15` holds there (64 files, all OK) [F].

| Row | Run | Result |
| - | - | - |
| V1 | `test_cpu_stage2.py` | 8 tests pass (#169) [F] |
| V6 | `w09-repeat.json`, five shards | all five: M1 to M3 pass, **M4 finding**, **M6 finding** `CPU 2: CPU stage-2 drift`, on both CPU models; no disagreement [F] |
| V2 | baseline and controls (C2-BASELINE, W35, W36, v3's C6 and C7), in the written run | M6 `matches the grant` on both CPU models; every verdict as recorded [F] |
| V3 | C2-W09, in the written run | M6 pass before, **finding now**: `CPU 2: CPU stage-2 drift` on both CPU models; holds [F] |
| V4 | the other 39 written cases and the 4 corrected cases | the same verdict as the recorded runs for every case and method; the written run's disagreements are the recorded ones (W03, W07, W12, W23 do not build; W08 halts before stage-2 setup) [F] |
| V5 | M6 run time (`score.py`) | written set: median 0.9 s before, 1.2 s now; corrected set: 0.8 s, 0.9 s; grows by under 1 s; holds [F] |

Recorded runs compared: the written run of 2026-11-21 06:57 UTC at
`54e3bd9` and the corrected run at `bd5822e` (RESULTS-C2.md).

**Correction to the section above (and #168).** W09 is not an
equivalent mutant: the five V6 runs and V3 show CPU 2 on partition 1's
tables, and partition B's check fails. The Mac run that suggested
otherwise tested the unchanged tree: its build directory was configured
from the repository, not from the patched copy (`CMakeCache.txt`
`CMAKE_HOME_DIRECTORY`), and the copy's `src/core/partition.c:416` still
reads `hv_arch_stage2_enable(PARTITION_B_ID, PARTITION_B_ID)` [F]. The
inference from `context.S` was built on that run and is withdrawn; why
the world switch does not restore CPU 2's VTTBR_EL2 is not examined here
[I]. QEMU 11.1 was not the cause. Lesson for the method: a local
sanity run of a case goes through `run_case.py` (which exports, patches
and builds one tree), never a hand-made build.

With v15, read-back detects W09; the blind spot named in RESULTS-C2.md
(E5) is closed for CPU-ASSIGN faults whose wrong VTTBR_EL2 is still
loaded when read-back pauses the machine [F for W09, I in general]. C2's
prediction count becomes 179 of 180 under v15 (W08 alone) [F].
