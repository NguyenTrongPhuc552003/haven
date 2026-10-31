# BUGFIX: MM1 stage-2 permission bits + MM2 execute-never encoding

Classification: BUGFIX. Owner: HAVEN. Approved by the workspace owner
2026-10-31 after decisions D1-D4 on the
[MM1 proposal](../20261031-mm1-semantics/PROPOSAL.md). **Uncommitted.**
Base: revision `f775304ce9443dc9861ba1880e4208c7ae19b314` plus the working-tree
diff in `base-worktree.patch` (SHA-256 `933ae697…f748`).

## Change ([mm1-bugfix.diff](mm1-bugfix.diff), 22 files, against the base working tree)

- `include/haven/stage2.h`: portable `HV_MEM_R/W/X/DEVICE` bits,
  `HV_MEM_RWX`, `HV_MEM_DEVICE_RW`, `hv_stage2_attrs_valid()`.
- `src/core/mm/stage2.c`: `hv_stage2_map_partition` rejects invalid `attrs`
  with `HV_EINVAL` before any state change. Valid means: no unknown bits,
  `HV_MEM_R` present, never `DEVICE` with `X`.
- `arch/arm64/include/asm/s2_attrs.h` (new): pure `hv_s2_hw_attrs()`
  translation per the proposal's table.
- `arch/arm64/mm.c`: uses the translation; comment corrected.
- `arch/arm64/include/asm/page.h` (MM2): `XN[1:0]` field; execute-never is
  `0b10` (bit 54 only). `S2_XN_ALL` kept as an alias with the corrected value.
  `S2_XNX`, `S2_UXN`, `S2_PXN` removed (no users).
- Callers: `src/core/partition.c` and 13 test files: `0` → `HV_MEM_RWX`,
  `1` → `HV_MEM_DEVICE_RW` (38 + 3 sites). Behaviour preserved for both.
- `tests/unit/test_stage2_attrs.c` (new), registered in `tests/CMakeLists.txt`
  and `scripts/dev/test.sh`.

**Scope note for the owner:** the validator requires `HV_MEM_R`, which also
rejects execute-only (`X`) and device-only values. The approved forbidden list
named 0, unknown bits, `DEVICE|X` and `W` without `R`; execute-only was not
named, but it has no row in the approved translation table, so it is rejected
(fail closed). **Confirmed by the workspace owner 2026-10-31.**

## Validation (experimental observation, 2026-10-31)

| Check | Environment | Result | Evidence |
| - | - | - | - |
| Base matches the Mac working tree | clone + `base-worktree.patch` | 5 key file hashes equal | this README |
| Baseline host suite, before change | Linux x86_64, GCC, CMake 3.28, Ninja | 19/20, `bench_temporal_isolation` timeout (same as Mac) | - |
| Host build after change | same | rc 0, no warnings | `host-build.log` |
| Host suite after change, `ctest --timeout 10` | same | 20/21; only `bench_temporal_isolation` timed out | `host-ctest.log` |
| New test can fail | same | XN reverted to `0b11`: abort; validator accepting 0: abort | - |
| `scripts/dev/test.sh` path | same | `test_stage2_attrs: all checks passed`; script then stalls in the known slow benchmark (stopped) | - |
| 20 Mac files unchanged before write | Mac (VM shell) | all hashes equal to base | - |
| ARM64 compile-only: `mm.c`, `stage2.c`, `partition.c` | Mac VM, `aarch64-linux-gnu-gcc` 11.4.0, HAVEN ARM64 flags (`-Werror -ffreestanding -march=armv8-a ...`) | rc 0 for all three | - |
| Full ARM64 build and link, `qemu-virt` MinSizeRel | Mac VM, `aarch64-linux-gnu-gcc` 11.4.0 (Ubuntu); the exact commands CMake generates for this config, replayed by `arm64-replay.sh` | rc 0; `haven.elf`, `guest_a.elf`, `guest_b.elf` linked; only warnings: 3× build-id discarded. `haven.elf` SHA-256 `f936494c…032f` | `arm64-build.log` |
| MM2 in the binary | `objdump -d` of `hv_arch_stage2_map` | execute-never applied as `orr #0x40000000000000` (bit 54 only); no `0x60…` value; S2AP chosen between `0xc0` (RW) and `0x40` (RO) | `hv_arch_stage2_map.dis` |
| **Pinned ARM64 build and link**, `qemu-virt` MinSizeRel | macOS, owner's terminal; `aarch64-unknown-linux-gnu-gcc` 15.2.0 (Homebrew Cellar path in the log); CLAUDE.md Commands, fresh build dir `/tmp/haven-arm64-mm1` | 32/32 steps; `haven.elf`, `guest_a.elf`, `guest_b.elf` linked; 0 errors; 6 linker warnings (3× RWX LOAD segment, 3× build-id), the same set the Stage 0 baseline records. `haven.elf` text 25 380 B. Exit code not captured (output piped to `tee`); success read from the log | `arm64-pinned-build.log` (SHA-256 `a7d8c801…378d`) |

## Not done

- **QEMU L2/L3 tests** (read-only write fault, XN instruction fault, on
  `cortex-a72` and `-cpu max`): need a reproducible QEMU launch and a
  lower-EL abort reporting path. Not written.
- **Arm ARM check** of the `XN[1:0]` table (MM2 still relies on QEMU's
  implementation).
- No isolation claim follows from host tests: they compile the ARM64 branches out.
