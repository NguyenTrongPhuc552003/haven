# Linux chain case set v1 (proposal)

**Status:** PROPOSAL, 2026-11-06, drafted by Claude (owner approval of the
step: "Approve", item 3). Nothing below has been run. The owner marks each
predicted verdict, adds held-out cases, and the set is frozen by SHA-256
before the first run, as for A1 case set v3
(`../20261102-a1-cases-v3/CASES-v3.md`).

## What this set tests

L4 (`../20261104-linux-partition/RESULTS-L4.md`) ran the chain once, with
one mismatch planted by Claude. This set asks which mismatches between the
guest description (T), the driver's demand (D), the grant (G) and the
enforced state (H) each check finds, on the Linux partition:

| Check | Compares | Tool |
| - | - | - |
| demand check | D with G | `tools/readback/demand_check.py` |
| read-back | H with G | `tools/readback/readback.py` |
| boot outcome | does Linux reach `KDAL-DEMAND-END` and power off | UART log |
| HAVEN log | denied accesses, undelivered interrupts, trapped registers | UART log |

Not tested, and why: **D within E** (no operator envelope E exists for the
Linux partition yet; the charter's E is a build-time input still to be
written) and **T exactly G** (no checker compares the device tree with the
grant). Cases T4 and T5 below show what that missing checker would have
to catch.

## Method

Every case is L4 (`run_l4.sh`, same kernel, `kdal.ko`, init, QEMU, virtio
device) with one change, in the device tree (T), the grant file (G) or
HAVEN's source (H). Read-back reads at `KDAL-DEMAND-END` or after 60 s.

## Cases and Claude's predictions (owner: Y / N / ? per row)

| Case | Changed | Seeded change | Demand check | Read-back | Boot | HAVEN log |
| - | - | - | - | - | - | - |
| T1 | T | KDAL's node over the granted UART (0x09000000, SPI 1) instead of the GPIO | no finding | matches | completes | nothing new |
| T2 | T | KDAL's node over the UART, 2 pages (0x2000) instead of 1 | unmet MMIO demand (second page) | matches | completes | nothing new |
| T3 | T | KDAL's node over the GPIO registers but with the UART's interrupt (SPI 1) | unmet MMIO demand only | matches | completes | nothing new |
| T4 | T | memory node 1 GiB instead of 512 MiB | no finding | matches | does not complete: Linux uses RAM HAVEN never mapped | denied stage-2 accesses above IPA 0x60000000 |
| T5 | T | timer node lists the virtual timer as PPI 12 (INTID 28) instead of 11 (INTID 27) | as L4 (2 findings) | matches | does not complete: no timer interrupt | none (28 is not owned, the emulated GIC ignores its enable) |
| G1 | G | grant also gives partition A the GPIO page and SPI 39 | no finding | missing binding (GPIO page) and SPI 39 not enabled | completes | nothing new |
| G2 | G | grant omits PPI 27 on CPU 0 for partition A | as L4 (2 findings) | PPI 27 enabled on CPU 0 but not granted | completes | nothing new |
| H1 | H | GIC model enables any interrupt the guest asks for, owned or not | as L4 | matches (Linux requests no interrupt outside its grant, so nothing changes) | completes | nothing new |
| H2 | H | Linux profile maps the GPIO page into partition A's stage-2 | as L4 (2 findings: the grant still lacks it) | extra binding (GPIO page not granted) | completes | nothing new |
| H3 | H | GIC model leaves owned interrupts enabled at start instead of disabled | as L4 | matches (Linux enables them anyway) | completes | nothing new |
| C1 | control | device-tree nodes reordered (KDAL's node first) | as L4 | matches | completes | nothing new |
| C2 | control | initramfs at IPA 0x4c000000 instead of 0x48000000 | as L4 | matches | completes | nothing new |

"As L4" means the L4 result: two findings (unmet MMIO demand
0x9030000, unmet interrupt demand 39), exit 1.

H1 and H3 are expected blind spots of end-state checks: the defect is
real but the guest never exercises it, so no check sees it. T4 and T5 are
the cases a T-against-G checker would catch before boot; today only their
symptoms show.

## What the owner reviews

1. Each row: Y (prediction right), N (and what it should be), or ?.
2. Held-out cases: 2 to 4 changes of your own, written down and hashed
   before Claude sees them. The map and the mechanics are in
   `../20261102-a1-cases-v3/HELDOUT-GUIDE.md`; for this set the
   device tree (`run_l4.sh` writes it from `partition-a.dts`), the grant
   (`grant-l4.json`) and the Linux-profile code (`src/core/partition.c`,
   `src/core/irq/vgic.c`) are also fair places.
3. Then Claude writes the case runner, freezes the inputs
   (`freeze/SHA256SUMS`) and runs all cases once.
