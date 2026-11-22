# Read-back's board path: addresses from the platform (design)

**Status:** design and expectations, 2026-11-23, before any code.
Owner-approved ("Approve", 2026-11-23: read-back's board path, an open
item of G3 since the D5 record).

## Problem [F]

`tools/readback/readback.py` holds qemu-virt's addresses as constants:
HAVEN's image (0x8000_0000, 8 MiB), the SMMU (0x0905_0000), the GIC
distributor and redistributors (0x0800_0000, 0x080A_0000, frames by a
fixed 128 KiB stride) and four CPUs. On CIX P1 every one differs
(`src/platform/cix-p1/memory.h`: image 0xF000_0000, SMMU0 0x0B01_0000,
GIC-700 at 0x0E01_0000 and 0x0E09_0000, twelve CPUs, the boot CPU is
core 4). Its redistributor frames may be 256 KiB (GICv4) and are found by
`GICR_TYPER` affinity in HAVEN (D2), not by stride; historical case H5
(the stride revert) is the board fault this matters for (board plan,
Amendment 2, B-H5). And read-back can only take its input from a QEMU it
boots: the EL3 dump (`--el3`) is parsed from the UART log of that QEMU,
so a dump captured on a board has no way in.

## Design [P]

1. **Platforms** (`tools/readback/platforms.py`): per platform the image
   base and size, SMMU base, GIC distributor and redistributor bases, the
   logical CPU count and each logical CPU's MPIDR. qemu-virt's values are
   today's constants; cix-p1's come from its `memory.h` and `platform.c`.
   A test reads both headers and fails on any difference.
2. **`--platform NAME`** (default `qemu-virt`, so every existing call is
   unchanged).
3. **Redistributor frames by affinity**: walk the frames from the
   redistributor base (128 KiB, or 256 KiB when `GICR_TYPER.VLPIS` is
   set) until `GICR_TYPER.Last`, and give each logical CPU the frame whose
   `GICR_TYPER` affinity equals its MPIDR, as HAVEN does. In a dump, the
   frames are those the dump holds. On qemu-virt this gives the frames
   the stride gives today.
4. **`--dump FILE`**: read-back of a dump saved elsewhere (the EL3
   service's `RB` lines, as `--el3` parses them from QEMU's log), without
   booting anything. This is the board's way in, whatever channel
   captured the dump.

Not in scope: producing a dump on the board (the vendor BL31 and J21 are
the board plan's Amendment 1 questions); the EL3 service's own addresses
(`tools/el3-readback/`) stay qemu-virt's until a board BL31 can be built.

## Expectations

| Row | Check | Expected |
| - | - | - |
| B1 | new unit tests | the platform table equals both platforms' headers; the frame walk maps logical CPUs correctly for 128 KiB and 256 KiB frames and for a boot CPU that is not core 0 (cix-p1's map); a missing frame is a probe failure for that CPU, not a silent default |
| B2 | `--dump` on a saved qemu-virt EL3 dump of the unchanged tree | the same report as `--el3` gave live for it: enforced state matches the grant |
| B3 | campaign scenario 1 after the change | every M6 and M7 verdict and evidence line as in v16's run 45047873482 |
| B4 | the other read-back users in CI (Linux job, R1 demo, UEFI checks) | pass unchanged |
| B5 | a synthetic cix-p1 dump (twelve 256 KiB frames, core 4 first, the v1 grant's state) | `--platform cix-p1 --dump` matches `grant-cixp1-v1.json`; one PPI disabled in core 8's frame is reported on logical CPU 8 |

## Code and B1, B2, B5 (2026-11-23, on the Mac) [F]

`tools/readback/platforms.py` (the table and the frame walk),
`readback.py` v17 (`use_platform`, `--platform`, `--dump`, frames by
affinity in both the monitor and the dump path, a CPU without a frame is
a probe failure).

| Row | Result |
| - | - |
| B1 | `test_platforms.py`: the table equals both platforms' `memory.h` and `platform.c`; frames found for 128 KiB frames, for 256 KiB frames with core 4 first, and a missing frame left empty. 4 of 4 pass. **Holds** |
| B2 | the unchanged tree under TF-A (QEMU 11.1.3): `--el3` live, then `--dump` of the dump that run saved, same build: both `enforced state matches the grant`, and every line of the two reports is the same except the lines about the trigger and QEMU's device tree, which a dump has no QEMU for. A monitor-mode run (`qemu-psci`, max) also matches the grant. **Holds** |
| B5 | `test_board_dump.py`: a synthetic cix-p1 dump built from `grant-cixp1-v1.json` has no finding; with PPI 27 off in core 8's frame the only finding is `CPU 8 PPI 27: missing interrupt binding`; a dump without core 11's frame is a probe failure. 3 of 3 pass. **Holds** |

B3 and B4 are judged on the campaign run and the CI jobs after merge.
