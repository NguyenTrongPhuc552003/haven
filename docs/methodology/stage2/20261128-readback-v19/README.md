# Read-back v19: HAVEN's interrupt ownership table against the grant (design)

**Status:** design and expectations, 2026-11-28, before any code.
Owner-approved ("Approve", 2026-11-28).

## Why

N3-W1 (`../20261128-readback-n3/`) took vCPU 2's timer PPI away and
read-back passed it: read-back v8 takes "did the guest enable it?" from
HAVEN's GIC model, and the model drops an enable of an interrupt HAVEN
does not think the partition owns (`src/core/irq/vgic.c`, `set_enable`).
The fault hides behind HAVEN's ownership table, the state it corrupts
[F]. Read-back already reads HAVEN's own state where the hardware cannot
say enough (the GIC model, v8; the vCPU table, v12; `cpu_partition`,
v10). The ownership table is the same kind of state.

## The rule [P]

Read-back reads HAVEN's three ownership arrays from the image
(`src/core/irq/ownership.c`, static, found by symbol like
`cpu_partition`):

- `ppi_owner_map[16][16]`: per CPU, the owner of PPIs 16 to 31;
- `irq_owner_map[1024]` and `irq_target_cpu_map[1024]`: per SPI, the
  owner and target CPU.

Against the grant:

| Case | Finding |
| - | - |
| a PPI granted to the partition on CPU c (`ppis` or `guest_enabled`) whose table entry is not that partition | "missing interrupt binding" (`CPU c PPI n: HAVEN's table holds partition X`) |
| a table entry giving a partition a PPI the grant does not give it on that CPU | "interrupt ownership drift" |
| a granted SPI whose table owner or target CPU differs from the grant | "missing interrupt binding" or "interrupt routing drift" |
| a table entry giving a partition an SPI the grant does not | "interrupt ownership drift" |

HAVEN's own interrupts (`haven_ppis`, `maintenance_ppi`, SPIs granted to
partition 0) are not in the partition tables and are not checked here.
The EL3 path (M7) dumps HAVEN's image as well, so the rule runs there
too. An image without the symbols (an older build) skips it with an
observation.

## Expectations

| Row | Check | Expected |
| - | - | - |
| V1 | read-back unit tests | a table that matches: no finding; each row of the table above gives its finding; a missing symbol: an observation, no finding; all earlier tests pass |
| V2 | the controls: scenario 1 (six), scenario 2 (three), `linux3` (three) | every method passes, M6 and (scenario 1) M7 included (no false finding from the new rule) |
| V3 | N3-W1 again | M6 finds it: `CPU 3 PPI 27` missing interrupt binding, HAVEN's table holding no owner |
| V4 | the campaign's case sets again with v19 (scenario 1, 106 cases; scenario 2 without S2-R38, 42) | every M6 verdict that changes from v18 is listed and explained; the frozen campaign's figures stay as recorded, v19's are reported beside them [?: no prediction of the count] |
| V5 | read-back's time | the M6 median rises by less than 1 s |
