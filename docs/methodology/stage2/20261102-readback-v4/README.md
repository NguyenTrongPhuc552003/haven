# Read-back v4: halted HAVEN, granted PPIs, interrupt priorities

**Status:** EXPERIMENT, observed 2026-11-02. Tool change approved by the
owner (2026-11-02). Expectations by Claude, committed with the runner in
`a9bf185` before the run. These are regression checks, not held-out cases:
Claude knew the earlier outcomes of R2, R3, I7 and K3 when writing them.
The priority values in both v4 grants were delegated to Claude by the owner (2026-11-02); the decision is below.

## Changes (`tools/readback/readback.py`, haven `a9bf185`)

| Gap | Seen in | v4 behaviour |
| - | - | - |
| RB1 | held-out R3 | A stage-2 root that is zero or outside HAVEN's image is a probe failure (exit 2) with a report ("HAVEN halted before stage-2 setup"), not a Python crash that a runner can mistake for findings |
| RB2 | partition B K3 | A granted PPI not enabled on its CPU is a missing interrupt binding, as SPIs already were |
| Priorities | case I7, held-out R2 | When the grant's `gic` section has `priority`, every granted SPI (GICD_IPRIORITYR) and PPI (GICR_IPRIORITYR) is compared with it; a mismatch is interrupt priority drift, marked "outranks HAVEN" when a partition interrupt is at or above HAVEN's priority |

Read-back v3 stays reproducible from haven `ce220bb` (case set v3 freeze).
The runner here takes findings only from `readback.json`; an exit status
without a report is shown as "no report".

## Grants (priority values decided by Claude, delegated 2026-11-02)

`grant-v4.json` is the v3 grant and `grant-partb-v4.json` the partition B
grant, each plus `"priority": {"haven": "0x40", "partition": "0xa0"}`: what
HAVEN programs today (GIC_PRIO_HYPERVISOR for its own SMMU interrupt,
GIC_PRIO_LINUX for every partition interrupt).

Why one value for both partitions, including real-time partition B: each
partition has CPUs of its own, so a physical priority only has to order a
partition's interrupts below HAVEN's own on the same CPU; ordering between
partitions never happens in one CPU interface. Inside a partition the guest
sets its own virtual priorities. `GIC_PRIO_RTOS_HIGH` (0x60) becomes
meaningful only when two partitions share a CPU, which the timing stage may
introduce; the grant will be revisited then.

## Results (`run_v4.py`, `runs/results.md`; 16 of 16 as expected, both CPUs identical)

| Check | Code | v3 result | v4 result |
| - | - | - | - |
| V0 | `ce220bb` | matches | matches (SPI 33 at 0xa0, SPI 106 at 0x40) |
| R1 | `ce220bb` + R1 | matches | matches |
| R2 | `ce220bb` + R2 | matches (blind spot) | **SPI 33: priority drift, 0x0, outranks HAVEN** |
| R3 | `ce220bb` + R3 | tool crash, counted as findings | **probe failure: HAVEN halted before stage-2 setup** |
| R4 | `ce220bb` + R4 | 7 findings | the same 7 findings |
| I7 | `ce220bb` + I7 edit | matches (blind spot) | **SPI 33: priority drift, 0xf0** |
| W0 | HEAD, qemu-psci | (new) | matches, including PPI 27 on CPU 2 at 0xa0 |
| W3 | HEAD + K3 edit | matches (blind spot RB2) | **CPU 2 PPI 27: missing interrupt binding** |

R2's effect is now measured, not inferred: the partition's UART interrupt
sits at priority 0x0, above HAVEN's 0x40.

## Limits

Priorities are checked only for granted interrupts; an ungranted interrupt
is already drift if enabled. The CPU interface (ICC_PMR, list registers,
virtual priorities) is still not read: a partition interrupt masked by
HAVEN's PMR (I7's 0xf0 equals the PMR) is caught here only through the
distributor value. Same platform limits as v3: QEMU virt, one security state.
