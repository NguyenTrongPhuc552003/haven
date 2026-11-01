# Proposal: a read-only region in the QEMU grant

**Status:** proposed 2026-11-02; needs owner approval because it changes the
grant G of the QEMU test platform. Nothing is implemented.

## Why

The scored case set showed a blind spot (`RESULTS.md` section 6, control C2):
the current grant has no read-only region, so a defect that makes every
mapping writable (the MM1 class) leaves the enforced state equal to the grant,
and read-back cannot see it. A grant with one read-only region makes that
class of drift visible.

## Change (grant only, option A)

Add one region to partition A in `src/core/partition.c`:

| Field | Value | Reason |
| - | - | - |
| name | A RO | |
| IPA | `0x7000_0000` | unused in both partitions' IPA maps |
| PA | `0xA480_0000` | first free RAM after partition B (B ends at `0xA480_0000`; QEMU RAM ends at `0xC000_0000`) |
| size | 2 MiB | one L2 block, the path most RAM uses |
| attrs | `HV_MEM_R` | read-only, never executable (XN `0b10`) |

No guest code changes: partition A never touches the region, so the boot
check and current behaviour stay as they are. Read-back expects
`s2ap=1, memattr=0xF, sh=3, xn=0b10, af=1` for it.

**Option B (later, separate approval):** partition A also writes to the
region once, so the write is denied and logged. That would let the boot check
see the MM1 class too; option A keeps the first experiment about read-back
alone.

## Expected effects (to be confirmed by a run)

| Case | Today | With the read-only region |
| - | - | - |
| C1 baseline | H matches G | H matches G (new region read back correctly) |
| C2 MM1 revert | H matches G | **access drift on A RO** (read/write instead of read-only) |
| D5 Normal forced read-only | access drift on A RAM, B RAM | unchanged; A RO already read-only |
| Unit tests | catch C2 | unchanged |

## What approval covers

1. Adding the region (option A) to the QEMU grant on `main`.
2. A grant v2 file (`expected-G.json` plus the region) and a new frozen case
   set in which C2 becomes a seeded fault with the expected verdict above. The
   owner reviews that case set before it runs, as before.
