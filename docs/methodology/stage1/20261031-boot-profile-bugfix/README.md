# BUGFIX: PSCI boot profiles (B1) + stop on guest instruction abort (EX2)

Classification: BUGFIX. Owner: HAVEN. Approved by the workspace owner
2026-10-31, implementing decisions E2 and E3 from the
[QEMU readiness audit](../20261031-mm1-qemu-readiness/AUDIT.md). **Uncommitted.**
Base: `f775304` + `base-worktree.patch` (SHA-256 `8f40b099…5120`) +
`base-untracked-sources.tar` (MM1's new files; SHA-256 `e893ceb0…457d`), i.e.
the working tree after the [MM1 BUGFIX](../20261031-mm1-bugfix/README.md).

## Change ([boot-profile-bugfix.diff](boot-profile-bugfix.diff), 5 files)

**Part 1, B1 / E2: explicit PSCI boot profiles**
- `CMakeLists.txt`: cache option `HAVEN_BOOT_PROFILE` = `qemu-direct`
  (default) | `qemu-tfa`, for `PLATFORM=qemu-virt` only. It selects
  `HAVEN_PSCI_CONDUIT_NONE` or `HAVEN_PSCI_CONDUIT_SMC`; an unknown profile is
  a configure error. Other platforms get `HAVEN_PSCI_CONDUIT_HVC`, which keeps
  their historical (unverified) behaviour. The selected conduit is printed at
  configure time.
- `arch/arm64/boot.S` `hv_arch_psci_cpu_on`: `NONE` returns PSCI
  NOT_SUPPORTED (-1) without any firmware call; `SMC` issues `smc #0`; `HVC`
  issues `hvc #0`; no conduit defined is a build error. A non-zero result
  still goes through the existing error log, and `partition.c` already keeps
  partition B inactive and the mailbox unreleased on failure.

**Part 2, EX2 / E3: stop the partition on a guest instruction abort**
- `include/haven/el2_exceptions.h`, `src/core/exc/el2_exceptions.c`: new pure
  function `hv_s2_fault_action(esr)` → `HV_S2F_NOT_S2` | `HV_S2F_SKIP`
  (lower-EL data abort, as before) | `HV_S2F_STOP` (lower-EL instruction
  abort). `hv_handle_sync` logs as before; on `STOP` it logs
  `HAVEN: partition stopped after stage-2 instruction abort; CPU parked` and
  parks that CPU in a `wfe` loop at EL2 instead of `ELR += 4`. The EX1 caveat
  about the logged IPA is noted in a comment.
- `tests/unit/test_el2_exceptions.c`: `test_s2_fault_action` (data aborts
  skip; instruction aborts stop; S1PTW, out-of-range FSC, HVC and same-EL
  aborts get normal handling).

## Validation (experimental observation, 2026-10-31)

| Check | Environment | Result | Evidence |
| - | - | - | - |
| Base matches the Mac | 13 relevant files hashed on both sides | equal | - |
| Host build | Linux x86_64, GCC, CMake 3.28, Ninja | rc 0, no warnings | `host-build.log` |
| Host suite, `ctest --timeout 10` | same | 20/21; only `bench_temporal_isolation` timed out (as before) | `host-ctest.log` |
| New test can fail | same | old `ELR += 4` for instruction aborts restored: abort | - |
| Bad profile rejected | CMake configure, `HAVEN_BOOT_PROFILE=bogus` | configure error naming the valid profiles | - |
| New tests on the Mac sandbox | Ubuntu aarch64, GCC 11.4 native | `test_el2_exceptions` and `test_stage2_attrs` pass | - |
| ARM64 build, `qemu-direct` | Mac sandbox, `aarch64-linux-gnu-gcc` 11.4.0, CMake-generated commands (`arm64-replay-qemu-direct.sh`) | rc 0; 3 build-id warnings; `hv_arch_psci_cpu_on` has `mov x0, #-1` and no `hvc`/`smc` | `arm64-build-qemu-direct.log` |
| ARM64 build, `qemu-tfa` | same (`arm64-replay-qemu-tfa.sh`) | rc 0; 3 build-id warnings; `hv_arch_psci_cpu_on` has `smc #0` | `arm64-build-qemu-tfa.log` |

| **Pinned ARM64 builds**, both profiles | macOS (run via Desktop Commander), `aarch64-unknown-linux-gnu-gcc` 15.2.0 (Homebrew), CMake + Ninja, fresh `/tmp/haven-arm64-<profile>` | `BUILD_RC=0` for both (exit code captured); 6 linker warnings each (RWX + build-id, as in the Stage 0 baseline); configure prints the intended conduit | `arm64-pinned-qemu-direct.log`, `arm64-pinned-qemu-tfa.log` |
| Pinned disassembly | Homebrew `aarch64-unknown-linux-gnu-objdump` | direct: `mov x0, #-1`, no firmware call; tfa: `smc #0`; MM2 `orr #0x40000000000000` present. `haven.elf` SHA-256 direct `cd3b284a…e331`, tfa `574d609e…e1dc` | - |
| **QEMU boot, qemu-direct, `-cpu cortex-a72`** | QEMU 11.1.3 (Homebrew), `virt,virtualization=on,gic-version=3,iommu=smmuv3`, 4 CPUs, 2 GiB, 20 s (`qemu-direct-run.sh`) | HAVEN boots; `PSCI CPU_ON error -1 (NOT_SUPPORTED)` → `partition B inactive`, no fatal exception; partition A runs at EL1 and prints via its `DEVICE_RW` UART; A's access to IPA `0x60000000` denied (ESR `0x93c08006`: EC `0x24` data abort from lower EL, FSC `0x06` translation fault level 2, read) and A continues (`Post-fault check OK`); stopped by the timeout | `qemu-direct-uart.log` |
| QEMU boot, qemu-direct, `-cpu max` (FEAT_XNX model) | same, `-cpu max` (`qemu-direct-run-cpumax.sh`) | same sequence and denial; only `PARange=6` differs | `qemu-direct-cpumax-uart.log` |

The one other `hvc` in `haven.elf` (both profiles) is `hvc #1` in
`hv_freertos_yield_budget_tick` (`src/guest/rtos/freertos_integration.c`), a
guest-to-hypervisor call, not PSCI. Out of scope.

## Not done

- QEMU run of the **qemu-tfa** profile: needs the TF-A build and its QEMU
  command recorded (ELMOS).
- QEMU 11.1.3 was used; the Stage 0 baseline records 11.1.2. The host QEMU was
  upgraded in between; record versions with every run.
- A read-only write test and an execute-never fetch test still need a test
  guest and partition config (readiness audit row 4).
- The `STOP` path has not executed: no guest instruction abort happened in
  these runs, and host tests compile the ARM64 handler out.
- B2/B3 (safe secondary entry) are unchanged, so a successful TF-A `CPU_ON`
  still runs the existing secondary path.
- EX1 (logged IPA for permission faults): no correction yet.
