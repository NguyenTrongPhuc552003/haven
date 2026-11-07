# EL3 read-back prototype: results

**Run:** 2026-11-08, macOS, QEMU 11.1.3, TF-A v2.12.0 with
`tools/el3-readback/`, haven `f2b1c53` (expectations committed there
before the run). Raw output: `runs/<case>/monitor/` and `runs/<case>/el3/`
(read-back text and JSON; `el3/uart1.log` is BL31's dump), and
`runs/results.md`. Absolute paths in the logs are replaced by
`<workspace>` and `<tmp>`; QEMU's dumped device trees are removed.

## Main result

| Case | Fault | Findings (both channels, identical) | Same |
| - | - | - | - |
| E0 | none | CPU 2 PPI 27 missing binding | yes |
| M3 | stage-2 execute bit | E0 + execute drift on A RAM and B RAM | yes |
| M4 | UART mapped Normal | E0 + memory-type and shareability drift on A UART, A PCI config, A PCI BAR; **+ "partition memory mapped at EL2" for those three device regions** | yes |
| M5 | partition B 2 MiB larger | E0 + extent drift on B RAM; **+ association drift on A RO** | yes |
| S8 | DMA grant read-only | E0 + DMA access drift on SID 8 | yes |
| I1 | SPI 33 routed elsewhere | E0 + **SPI 33 missing binding** (expected: routing drift) | yes |
| I4 | SPIs written Group 0 | E0 only | yes |
| I5 | all CPU 0 PPIs enabled | E0 + enable drift on CPU 0 PPIs | yes |
| S1 | CPU 0 recorded as partition B | E0 + CPU assignment drift on CPU 0; **+ SPI 33 missing binding** | yes |

**The EL3 dump and the QEMU monitor gave the same findings in all 9
cases** (main expectation held). This covers stage-2 (M3, M4, M5), the
SMMU stream table (S8), GIC enable and routing (I1, I5), and HAVEN's CPU
table (S1), read by BL31 without any help from HAVEN.

## Against the per-case expectations

5 of 9 held exactly (E0, M3, S8, I4, I5). 4 had more or other findings
than predicted, the same in both channels (so they are prediction errors,
not channel differences):

- **M5:** partition A's read-only region is placed after partition B and
  the shared region (`PART_A_RO_PA_BASE = SHARED_PA_BASE + SHARED_SIZE`,
  `src/platform/qemu-virt/memory.h`), so enlarging B moved it by 2 MiB:
  read-back correctly reports its new physical address as association
  drift. The prediction copied the 2026-11-02 expectation, written before
  that region existed.
- **M4:** the changed attribute macro also changes how HAVEN's EL2 map
  describes those device regions; read-back v5's EL2 check reports them.
  Not predicted.
- **I1 and S1:** SPI 33 ends up disabled instead of routed to the wrong
  CPU (I1), and disabled in S1 as well. Not investigated here; recorded
  as observed.

I4 held as reasoned: with two security states HAVEN's Non-secure writes
to `GICD_IGROUPR` do not reach the hardware, and the EL3 view shows Group
1 Non-secure. Under TF-A this fault cannot happen from EL2.

## What the prototype shows and does not

- **Shows:** firmware at EL3 can read everything read-back compares,
  convert the GIC to the Non-secure view, and reach the monitor's verdicts
  on these faults. In one respect it sees more: interrupt groups, which
  the monitor's Non-secure view cannot read under TF-A.
- **Does not show:** that it works on the CIX P1 (the board's BL31 must be
  rebuilt and flashed, with the owner's approval); an atomic snapshot (the
  other CPUs keep running during the dump); a read at a chosen instruction.
  The dump moves trust from HAVEN to our BL31 change; it does not remove
  it.

Dump size here: about 2,170 records, about 150 KB of text (E0: 2,165
records, 153,721 bytes). At 115200 baud (about 11.5 KB/s) on a board UART
that would take about 13 s.
