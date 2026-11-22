# M7 in the campaign: EL3 read-back on every case (design)

**Status:** design and expectations, 2026-11-22, before any code. The
owner left the choice to Claude ("just do it in your way"); C5 found E4
untested because M7 was never wired (`../RESULTS.md`).

## What exists [F]

`tools/el3-readback/` patches TF-A v2.12.0's BL31 (QEMU platform,
`EL3_READBACK=1`) to dump HAVEN's region, the GIC, the SMMU and the
interrupted CPU's EL2 registers on the secure UART after 8 s;
`readback.py --tfa DIR --el3` compares that dump with the grant. HAVEN's
`qemu-tfa` profile boots under it. On 9 cases (2026-11-08) the dump and
the QEMU monitor gave the same findings
(`../../20261108-el3-readback/RESULTS.md`).

## Design [P]

1. **CI**: the case job (Debian trixie) builds TF-A v2.12.0 with the
   patch once per job (`CROSS_COMPILE=aarch64-linux-gnu-`), cached by
   the patch's hash.
2. **M7** in `run_case.py` (scenario 1, when `--tfa DIR` is given):
   HAVEN built with `qemu-tfa`, then `readback.py BUILD GRANT OUT --tfa
   DIR --el3 --wait 40 --device ...` on cortex-a72 (as the prototype);
   verdict as M6's (finding, pass, error). A failed build is an error.
3. **E4 is read case by case**: M7's verdict against M6's on every case.

## Expectations

| Row | Check | Expected |
| - | - | - |
| T1 | the TF-A build in the container | BL1, BL2 and BL31 built with `EL3_READBACK=1`; the unchanged tree's M7 reads `matches the grant` |
| T2 | the 5 written controls and H6 | M7 pass on all six |
| T3 | E4: M7 against M6 on the other written cases | the same verdict, except where the EL3 channel differs by construction: **W09** (CPU-ASSIGN: the dump has only the interrupted CPU's EL2 registers, so v15's per-CPU VTTBR check does not run: M7 pass where M6 finds W09), **W27** and **v3's I8** (GIC-GROUP: under TF-A, HAVEN's Non-secure writes to `GICD_IGROUPR` do not reach the hardware, so the fault may not exist under TF-A, as I4 in the prototype); and W08 (HAVEN halts: error in both) |
| T4 | random and historical cases | no prediction per case; every M7/M6 difference is listed with a cause |
| T5 | run time | M7 adds under 90 s a case (one build, a 40 s wait) |

T3's exceptions are what I can derive from the code and the prototype's
record; any other difference is a finding about the EL3 channel or about
the two boot profiles (`qemu-psci` for M6, `qemu-tfa` for M7), named
before anything changes.

**Correction before code (2026-11-22):** T3 first listed W32 with W09.
W32 changes HAVEN's own CPU table (`hv_arch_set_cpu_partition`), which
is inside the dumped region, so M7 should find it like M6; only W09
(the VTTBR_EL2 value itself) is an exception. The TF-A build is not
cached: it takes about a minute in the job, less than a cache would save.

## Results (2026-11-22, run 44993759751 at `d7bbb9c`, the merge of #192)

[F] from the case files. TF-A v2.12.0 with the patch built in every case
job (T1's first half holds).

**T1 and T2 fail as written: a prediction error.** M7 reports one
finding on the unchanged tree and on all six controls: `CPU 2 PPI 27:
missing interrupt binding`. The prototype's own record had it already:
its E0 (unchanged tree under TF-A) showed exactly this finding in both
channels, "recorded here, not explained"
(`../../20261108-el3-readback/README.md`, Calibration). My T1 and T2
said "matches the grant"; they should have said "E0's finding only".
Every one of the 106 cases carries that line in M7 (or M7 errors).

**E4 with E0's finding as the baseline** (the prototype's E0, recorded
2026-11-08, applied after this run, so a post-hoc rule): M7's verdict
counts only findings other than that line.

| | Result |
| - | - |
| Controls (T2) | all six pass |
| M7 and M6 agree | **99 of 106 cases** (errors agree: HAVEN halts or does not build in both) |
| Differ, predicted (T3) | **W09** and **R42**: `CPU stage-2 drift` from v15's per-CPU VTTBR_EL2 check, which the dump cannot feed (one CPU's EL2 registers); **W27** and **R19**: interrupt group drift, which under TF-A's two security states HAVEN's writes cannot cause |
| Differ, not predicted [?] | **W14** and **R60** (M6: DMA enforcement drift on the PCI device's stream; M7 passes) and **W30** (M6: CPU 1 PPIs enable drift; M7 passes). Not examined yet; candidates are the `qemu-tfa` boot path for the DMA grant and the dump's redistributor coverage |
| T5 | M7's read takes 8.6 s median (9.1 s max; the 8 s timer). The slowest case job (about 9 cases, the TF-A build included) took 15.7 minutes; the added time per case was not measured on its own, so T5 is not settled |

So **E4 holds with named exceptions**: on 99 of 106 cases the EL3 dump
and the monitor reach the same verdict; four differences come from what
each channel can see (per-CPU VTTBR_EL2, interrupt groups under TF-A);
three are open and are named here before anything changes (E5).

**Next [P]:** M7's verdict in `run_case.py` subtracts E0's finding (as
M5 subtracts KDAL's GPIO), with a test; then W14, R60 and W30 are run
through both channels by hand to find the cause.

## The three open differences (2026-11-22)

- **W14 and R60** (M6: `DMA enforcement drift`; M7: no finding) [F,
  from the code]. Both leave stream 8's table entry at abort: W14 skips
  `smmu_v3_set_ste_partition`, R60 inverts the return-code test in
  `apply_dma_grants`. Read-back finds a granted stream that is not
  translated only through its list of DMA masters, and that list comes
  from the QEMU monitor (`query-pci`, `info qtree`, `dma_masters()`);
  the EL3 snapshot has no monitor, so `el3_snapshot()` sets both empty
  and the check never runs. The STE check that does run in EL3 mode
  looks only for entries that let DMA through without a grant. **A
  blind spot of the EL3 channel, in read-back, not in the dump**: every
  entry the dump needs is in it.
- **W30** (M6: `CPU 1 PPIs: interrupt enable drift`; M7: no finding)
  [I]. W30 routes partition B's timer PPI to CPU 1 instead of CPU 2.
  The dump covers all four redistributors (`RB_NCPUS 4`), so the
  register is there; under TF-A, CPU 1 is never powered on (it is not
  granted), and a likely cause is that HAVEN's enable of a PPI in a
  redistributor whose CPU is asleep does not take effect there, while
  under `qemu-psci` it does. Not checked; the record of the dump for
  W30 is not kept (only verdicts were uploaded).

**Proposal [P]: read-back v16.** Compare every granted stream with its
table entry directly (a granted stream must be translated by that
partition's tables), whether or not a DMA master is known. That closes
the EL3 blind spot for W14 and R60, and in monitor mode it adds a second
report of the same faults. Design and expectations as their own record
before any code.
