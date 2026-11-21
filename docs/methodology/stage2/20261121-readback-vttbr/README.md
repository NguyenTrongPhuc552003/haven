# Read-back v14: each CPU's stage-2 register, not HAVEN's table (design)

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
