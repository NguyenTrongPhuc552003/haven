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
