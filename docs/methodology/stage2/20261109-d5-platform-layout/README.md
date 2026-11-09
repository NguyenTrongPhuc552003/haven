# D5: reproducible images, partition A's layout from the platform, read-back width

**Status:** three owner-approved steps (2026-11-09), recorded here one
after the other: Step 1 (fixed build time), Step 2 (D5 proper: partition
A's layout from the platform, proposal 2 of D4), Step 3 (read-back reads
the emulated GIC's full width, proposal 3 of D4). For each step the design
and expectations are merged before its code, and results are added after
it by a later pull request; expectations are never edited after results.
No board exists: everything is checked by building, by host tests and in
CI. Labels: **[F]** fact from our code or records, **[O]** source
observation in `data/` (file and revision named), **[A]** assumption,
**[I]** inference, **[P]** proposal. Nothing here is a hardware
observation.

Baseline for all three steps: `main` at `2083d09` (pull request 60, D4
results), `aarch64-unknown-linux-gnu` GCC 15.2.0 on the macOS host (CI:
`aarch64-linux-gnu` GCC 15.2.0 on Ubuntu), `cloc` for
`tools/scripts/tcb-image-loc.py`. Five image variants are built for every
check, each in its own build directory outside the repository:

| Variant | Command | Baseline text / data / bss (bytes) |
| - | - | - |
| qemu | `cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct` | 43,140 / 144 / 5,251,064 |
| psci | `cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-psci` | 43,140 / 144 / 5,251,064 |
| qemu-linux | qemu plus `-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS=2` | 45,716 / 144 / 5,252,496 |
| cix | `cmake --preset arm64-cix-p1` | 43,196 / 144 / 5,251,064 |
| cix-linux | cix plus `-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS=2` (D4's B5) | 45,784 / 144 / 5,258,048 |

Baseline trusted code (`tcb-image-loc.py`): qemu 66 files, 7,455 code
lines; cix and cix-linux 67 files, 7,479. Host CTest 27 of 27 (as CI,
`bench_temporal_isolation` excluded); Python tests 53, all pass, none
skipped on the macOS host.

[F] With `SOURCE_DATE_EPOCH` set by hand to the baseline commit's time
(`git log -1 --format=%ct`, 1794203015), two builds of the five variants
at different times gave the same `haven.bin` for each variant (checked
before this record, as D4 did); without it they differ in the build time.

## Step 1: fixed build time

### Problem

D4's rows B1 and B2 were prediction errors [F]: `src/core/init.c` prints
`__DATE__ " " __TIME__` in the boot banner, so two builds of the same
source differ in three to eight bytes and "byte-identical" cannot be
checked without setting `SOURCE_DATE_EPOCH` by hand. GCC takes
`__DATE__` and `__TIME__` from `SOURCE_DATE_EPOCH` (in UTC) when the
variable is set [F: checked above with the host cross compiler, banner
`Nov  9 2026 05:43:35` for 1794203015, which is 12:43:35 +0700].

### Design

A new `cmake/reproducible.cmake`, included by `CMakeLists.txt` before any
target is defined, so it applies to every build (ARM64 images and host
tests, local, CI and records):

- If the environment sets `SOURCE_DATE_EPOCH` at configure time, that
  value is used (it must be a non-negative integer, else configure
  stops). Otherwise the value is the HEAD commit time,
  `git log -1 --format=%ct`, run in the source directory. Without git or
  outside a checkout (a source tarball) nothing is set and the build time
  is embedded as today; configure says so.
- Every C compile runs through
  `CMAKE_C_COMPILER_LAUNCHER = cmake -E env SOURCE_DATE_EPOCH=<value>`,
  followed by any launcher the user already set (for example `ccache`).
  `cmake -E env` is CMake's own portable tool, so the same line works on
  Linux, macOS and Windows; Ninja and Makefile generators honour the
  launcher (the presets use Ninja). Assembly sources do not use the date
  and get no launcher.
- Configure is re-run when HEAD moves: the git directory's `logs/HEAD`
  (or `HEAD` without a reflog) is added to `CMAKE_CONFIGURE_DEPENDS`, so a
  build after a commit or checkout picks the new commit time, and Ninja
  recompiles because the compile command changed.
- Configure prints `Haven SOURCE_DATE_EPOCH: <value> (HEAD commit time)`
  or `(environment)`.
- No C source changes. The banner still says `Build:`; with this change
  it shows the commit time in UTC, not the wall-clock time of the build
  (recorded as a limit, not changed in trusted code).
- CI: one more step in the ARM64 job, after the QEMU checks (minutes
  after the first build): the `qemu-direct` image is built again in
  `build-repro` and compared with `cmp` against `build/haven.bin`.

### Expectations

| Row | Check | Expected result |
| - | - | - |
| R1 | configure output, no `SOURCE_DATE_EPOCH` in the environment | `Haven SOURCE_DATE_EPOCH: <git log -1 --format=%ct> (HEAD commit time)` |
| R2 | two clean builds of the Step 1 code commit, at least one minute apart, in different build directories, no `SOURCE_DATE_EPOCH` in the environment | `haven.bin` identical (same SHA-256) for each of the five variants |
| R3 | each R2 image against a build of the same tree with `SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)` exported by hand (the D4 method) | identical |
| R4 | banner in `haven.bin` (`strings`) | `Build:   <Mmm dd yyyy hh:mm:ss>`, the HEAD committer time in UTC |
| R5 | `SOURCE_DATE_EPOCH=1793318400` exported at configure | configure prints `(environment)`; banner `Build:   Oct 30 2026 00:00:00` |
| R6 | sizes of the five variants | equal to the baseline table above (text, data, bss) |
| R7 | `haven.elf` (qemu), two clean builds in the same build directory path at different times | identical |
| R8 | a scratch clone: configure and build, then `git commit --allow-empty` with another commit time, then `cmake --build` | configure re-runs by itself, prints the new commit time, and the banner changes to it |
| R9 | trusted code | unchanged: qemu 66 files, 7,455 code lines; cix 67 files, 7,479 (no C or header file changes) |
| R10 | host CTest and Python tests | 27 of 27 pass; 53 Python tests pass, 0 skipped on the macOS host |
| R11 | CI on the code pull request | every job green, including the new step `Reproducibility (rebuild and compare haven.bin)` on Ubuntu's toolchain |

Verdict rule: a row holds if every stated value, identity and text
matches; anything else is a prediction error and is reported as such.
