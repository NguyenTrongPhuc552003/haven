# BUGFIX: host build regression in `hv_fatal_exception`

Classification: BUGFIX. Owner: HAVEN. Approved by the workspace owner on
2026-10-31, scoped to `src/core/exc/el2_exceptions.c`. **Uncommitted.**
Base revision `f775304ce9443dc9861ba1880e4208c7ae19b314`.

## Defect and fix

The Stage 0 diagnostic diff removed `(void)esr; (void)far; (void)elr;` while its
print sits inside `#ifdef HAVEN_ARCH_ARM64`. Host builds then failed `-Werror`
on unused parameters, so six test binaries did not build
([P-A0 baseline](../20261031-pa0/README.md)).

The fix restores those three casts. The diagnostic print is unchanged. Against
HEAD the file now differs only by the print ([el2_exceptions.diff](el2_exceptions.diff)).

## Validation (experimental observation, 2026-10-31)

Fresh disposable copy of the working tree. Host: Apple clang 21.0.0. ARM64:
aarch64-linux-gnu-gcc 15.2.0, `qemu-virt`, MinSizeRel. Copied file hash matched
the working tree.

| Check | Result | Log |
| --- | --- | --- |
| Host build, `ninja -k 0` | rc 0, 0 failures | `host-build.log` |
| Host suite, `ctest --timeout 10`, nothing excluded | 19 of 20 passed, 1 timeout | `host-ctest.log` |
| `test_isolation_negative`, `test_fault_injection` | ran, passed | `host-ctest.log` |
| ARM64 build | rc 0; 6 linker warnings, RWX segment and build-id only | `arm64-build.log` |
| Diagnostic string in `haven.elf` | present | - |

Two attempts failed before that build, both caused by the validation setup: a
relative `CMAKE_TOOLCHAIN_FILE` from outside the source tree left
`CMAKE_ASM_COMPILER` unset, and the copy omitted the guest linker scripts. An
absolute path and copying `*.ld` resolved them. `arm64-configure.log` and
`arm64-build.log` hold the successful attempt only.

## Not tested

- ARM64 runtime: no QEMU run, so the print was not exercised at a fatal fault.
- This fix makes no isolation claim. Passing host tests cover portable policy
  only, with the ARM64 branches compiled out.

## Separate diagnose-only item

`bench_temporal_isolation` hit the 10s limit in both full runs (test 18 of 20).
Not diagnosed and not changed. It may simply need longer than 10s: it is a
benchmark documented at 10 000 iterations per configuration. That is an
inference, not a result.
