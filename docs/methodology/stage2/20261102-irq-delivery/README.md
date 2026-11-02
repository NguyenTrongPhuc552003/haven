# GI1 fix and interrupt delivery to partitions (GI2)

**Status:** EXPERIMENT, observed 2026-11-02. Fixes approved by the owner.
Checks and expected outcomes by Claude; not blind.

## Changes

| Commit | Change |
| - | - |
| `de6291c` | GI1: GICD_CTLR written with the Non-secure layout (ARE_NS bit 4, EnableGrp1A bit 1, EnableGrp1 bit 0), as Linux `gic_dist_init` |
| `f6fe485` | GI2: EOImode 1 at EL2; a physical interrupt owned by the partition running on the CPU is injected through a list register with HW = 1 (the partition's EOI deactivates it); the owner is checked against the running partition, not inferred from the routing. Partition A's demo takes its UART interrupt (SPI 33) through the virtual CPU interface (`tests/demos/guest_a_irq.c`, `guest_a_vectors.S`) |
| `3fbafae` | An SPI that cannot be delivered is disabled in the distributor (found by J2 below: without it a level interrupt fired again at once and kept CPU 0 in HAVEN) |

## Results (`run_irq.py`, `runs/results.md`; both CPUs identical, 8 of 8 as expected after the third commit)

| Check | Change | Partition A / HAVEN report | Read-back |
| - | - | - | - |
| J1 | none | UART interrupt delivered (INTID 33); DMA checks as before | matches the grant (`grant-irq.json`) |
| J2 | HAVEN never finds the owner running (delivery broken) | NOT delivered; HAVEN: "IRQ 33 not delivered, disabled" | **missing interrupt binding** on SPI 33 |
| J3 | HAVEN's own table gives SPI 33 to partition B, still routed to CPU 0 | NOT delivered: HAVEN refuses to inject B's interrupt into A, and disables it | **missing interrupt binding** on SPI 33 |
| J4 | as J3, without the owner check | delivered to A | matches the grant |

## Observations

- **First run of J2 and J3 (before `3fbafae`):** partition A printed one
  character and stopped. The UART's level interrupt, neither injected nor
  cleared, fired again as soon as HAVEN completed it. The rule "disable what
  you cannot deliver" came from this observation.
- **J4 shows what the owner check prevents:** with HAVEN's table saying B
  owns SPI 33 but the GIC routing it to CPU 0, an implementation that infers
  the owner from the routing hands B's interrupt to A. Read-back cannot see
  this, because the grant (A owns SPI 33) and the hardware agree; only
  HAVEN's own table differs.
- GICD_CTLR still reads `0x53` after the GI1 fix: with one security state
  the old and new writes give the same value in QEMU. The fix cannot be
  shown in `qemu-direct`; it matters on a board with Secure firmware.

## Limits

Only SPIs routed to the boot CPU (partition A) are delivered; partition B's
CPU does not take physical interrupts yet. No virtual interrupt is
injected for a partition that is not running on the interrupt's CPU.
Read-back sees the distributor, not the list registers, so delivery itself
is shown only by the partition's report.
