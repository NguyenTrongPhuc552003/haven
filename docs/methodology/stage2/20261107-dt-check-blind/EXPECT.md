# Draw and predictions

**Drawn** 2026-11-07 with seed text `haven 9552d0e | owner 2026-11-07
01:51: 1. Approve 2. Approve 3. Approve 4. I had already deleted it on
Github permanently` (SHA-256 `4b89bfd7...0a92dd`, full value in
`draw.json`). `drawn/` holds the faults applied to the frozen T1 tree; the
runner applies the same faults to the tree it builds (only the initramfs
end differs, by the size of the new initramfs). Predictions by Claude,
committed before dt_check or any boot ran on these trees.

6 faults are labelled outside the grant, 6 inside. M04 and M12 are the
same fault (drawn with replacement).

| Fault | Fault drawn | Label | dt_check (predicted) | Boot (predicted) | Why |
| - | - | - | - | - | - |
| M00 | none (T1, control) | inside | matches | completes | as T1 in the case set |
| M01 | a second CPU node | inside | matches | completes; CPU 1 does not come online | partition 1 has CPUs 0 and 1 in the grant; HAVEN does not start a second CPU for partition A |
| M02 | initramfs range +16 MiB | inside | matches | **kernel panic** | the range is in granted RAM but the initramfs is not there: no `/init` |
| M03 | memory 496 MiB | inside | matches | completes | less RAM than granted |
| M04 | KDAL node renamed | inside | matches | completes | KDAL binds by compatible |
| M05 | KDAL registers at 0x09000800 | outside | memory finding (registers) | completes, no denied access | KDAL only maps its registers (L4: no access), as T2 |
| M06 | KDAL interrupt SPI 1 edge | inside | matches | completes, possibly a trigger type mismatch message | same interrupt as the PL011, which set it level first |
| M07 | KDAL registers 0x2000 | outside | registers finding | completes | as T2 |
| M08 | virtual timer PPI 10 | outside | PPI 26 not granted | **does not complete** | INTID 26 is HAVEN's; the emulated GIC ignores the guest enabling it, so the guest gets no timer interrupt (as T5) |
| M09 | virtual timer PPI 12 | outside | PPI 28 not granted | **does not complete** | T5 |
| M10 | three more CPU nodes | outside | CPU count finding | completes; CPUs 1 to 3 do not come online | |
| M11 | KDAL registers 0x800 | inside | matches | completes | |
| M12 | KDAL node renamed | inside | matches | completes | same as M04 |

Predicted score: 6 true positives, 6 true negatives. Predicted boots: 3
of 12 faulty trees do not boot to the end (M02, M08, M09); dt_check
reports 2 of them. M02 shows what containment cannot see: a tree inside
the grant that is still wrong.
