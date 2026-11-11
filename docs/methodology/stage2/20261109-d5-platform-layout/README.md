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

### Results (2026-11-09, macOS host and CI)

Expectations merged first (haven `22edfc3`, pull request 61); the code
after that (commit `2e5ed85`, merged as `53209bb`, pull request 62, CI
green on every job). Measured on `main` at `53209bb` (HEAD commit time
1794205236, 2026-11-09 13:20:36 +0700), no `SOURCE_DATE_EPOCH` in the
environment unless stated; build directories outside the repository.

| Row | Observed | Verdict |
| - | - | - |
| R1 | `-- Haven SOURCE_DATE_EPOCH: 1794205236 (HEAD commit time)` for all five variants (and for the host-test configure) | as expected |
| R2 | builds started at 06:21:13 UTC (set a), 06:22:09 (b) and 06:22:35 (c), each in its own directories: `haven.bin` SHA-256 (first 16 hex digits) qemu `02ac6876264da813`, psci `806ac057c07e2314`, qemu-linux `197361d5452dbb44`, cix `e752d10d191c9256`, cix-linux `553223211ea38b5f`, the same in a, b and c (a and c are 82 s apart; a and b 56 s) | as expected |
| R3 | the same tree with `SOURCE_DATE_EPOCH=1794205236` exported (configure says `(environment)`): the five images equal set a's (`cmp`) | as expected |
| R4 | `Build:   Nov  9 2026 06:20:36`, the commit time in UTC | as expected |
| R5 | `SOURCE_DATE_EPOCH=1793318400`: configure prints `(environment)`, banner `Build:   Oct 30 2026 00:00:00`; the image differs from set a's in 9 bytes, all in the banner | as expected |
| R6 | text / data / bss: qemu 43,140 / 144 / 5,251,064; psci the same; qemu-linux 45,716 / 144 / 5,252,496; cix 43,196 / 144 / 5,251,064; cix-linux 45,784 / 144 / 5,258,048 | as expected |
| R7 | `haven.elf` (qemu) built at 06:21 and rebuilt clean in the same directory about two minutes later: identical | as expected |
| R8 | scratch clone (not pushed): first build `Build:   Nov  9 2026 06:20:36`; after an empty commit dated 2026-11-10T00:00:00Z, `cmake --build` printed `[0/1] Re-running CMake...`, `Haven SOURCE_DATE_EPOCH: 1794268800 (HEAD commit time)`, and the banner became `Nov 10 2026 00:00:00` | as expected |
| R9 | qemu 66 files, 7,455 code lines; cix 67 files, 7,479 | as expected |
| R10 | CTest 27 of 27; Python 53 tests, OK, none skipped | as expected |
| R11 | pull request 62: all jobs green on Linux, macOS and Windows; the new step passed on Ubuntu (`aarch64-linux-gnu` GCC 15.2.0), both configures printing `Haven SOURCE_DATE_EPOCH: 1794205150 (HEAD commit time)` (the pull request's merge ref) | as expected |

**Verdict.** 11 of 11 rows hold as pre-registered. Observation outside
the rows: in CI the second build ran 18 s after the first (06:19:41 and
06:19:59 UTC), not "minutes" as the design said; the job is short. The
seconds of `__TIME__` still differ, so the step still tests the property.

Limits [I]: the banner shows the commit time, not when the image was
built; a build from uncommitted changes carries the HEAD time too (the
source then differs from that commit, so its image is not that commit's
image anyway). Outside a git checkout the build time is embedded as
before. Only `haven.bin` and `haven.elf` were compared; other outputs
(object archives of host tests) were not checked.

## Step 2: partition A's layout from the platform (D5)

### Problem

[F] `src/core/partition.c` includes `src/platform/qemu-virt/memory.h`
unconditionally and holds QEMU's vGIC IPAs (0x0800_0000, 0x080A_0000), so
a `PLATFORM=cix-p1` build carries QEMU's partition layout (D4, Part 1).
The `CIX_PART_A_*` constants of `src/platform/cix-p1/memory.h` are unused.
D4 also linked the cix-p1 image at 0x8000_0000, inside the firmware's
secure reservation.

### Owner decisions this step implements (2026-11-09)

1. Partition A on logical CPUs 0 to 7; RAM 2 GiB at 0x1_0000_0000,
   identity mapped (assumption until the board's EFI memory map); emulated
   GIC at GICD 0x0E01_0000 and GICR 0x0E09_0000; UART2 (0x040D_0000, INTID
   330) is Linux's console, UART3 HAVEN's (D3 grant v0, D4).
2. On cix-p1 partition B is **not mapped and not started** until it has
   its own record (the D3 grant gives it no memory).
3. **8 vCPUs is a separate later step** (`multi-vcpu.md` step 4). D5 keeps
   the vCPU count the code supports today: 1, or 2 with a Linux guest
   (vCPU 1 on `PART_A_CPU1`, default logical CPU 1).
4. cix-p1's link address becomes **0xF000_0000** (loading study,
   `../../../porting/cix-p1-loading.md`, pull request 64); qemu-virt stays
   at 0x8000_0000. No other part of that study (EFI loader, entry checks
   in `boot.S`) is done here.

### Design

- **Interface.** `partition.c` includes the selected platform's
  `memory.h` (`HAVEN_PLATFORM_CIX_P1`: cix-p1; otherwise qemu-virt, as
  today, also for the legacy i.MX builds). Each `memory.h` defines
  `PART_A_IPA_BASE`, `PART_A_PA_BASE`, `PART_A_SIZE`, `PART_A_UART_BASE`,
  `PART_A_UART_INTID` (a plain decimal, so it can also be printed),
  `PART_A_VGICD_IPA` and `PART_A_VGICR_IPA`. Optional, present on
  qemu-virt only: the read-only region (`PART_A_RO_*`), the DMA grant
  (`PART_A_DMA_*`) and partition B's memory and CPU (`PART_B_*`,
  `PART_B_CPU`). The QEMU vGIC IPAs move from `partition.c` to qemu-virt's
  `memory.h` with the same values and types.
- **cix-p1 values** (all already in `memory.h` from D4 or the decisions
  above): RAM IPA = PA = `CIX_PART_A_RAM_BASE`, size
  `CIX_PART_A_RAM_SIZE`; UART2; vGIC at the GIC-700 addresses; no
  read-only region and no DMA grant (D3: "no SMMU section"); no partition
  B. `CIX_PART_A_VCPUS` (8) stays the recorded target, not consumed.
  `PART_A_LAST_CPU` 7 bounds `PART_A_CPU1` (A's CPUs are 0 to 7).
- **Without partition B** (`PART_B_SIZE` undefined): B's regions, IRQs,
  budget, secondary entry and CPU_ON are compiled out;
  `partitions_launch` prints
  `HAVEN: partition B not started: no memory on this platform`.
  Read-back's grant for the board then has no partition 2 CPUs or PPIs; D3's
  grant v0, which lists B's CPUs 8 to 11 and their PPI 27, is left as it
  is (a record) and differs from the image on that point.
- **Fail closed:** a static assertion that `HAVEN_PART_A_DTB_IPA` lies
  inside partition A's RAM. D4's cix-p1 Linux build option
  `-DHAVEN_PART_A_DTB_IPA=0x50000000` is then refused at compile time; the
  cix-p1 Linux check uses `0x110000000` (A's base + 256 MiB, QEMU's
  offset), **a placeholder for compiling, not a load decision**.
- **Link address:** cix-p1's `HAVEN_LOAD_PA` becomes 0xF000_0000 and CMake
  passes `-Wl,--defsym=HV_LOAD_ADDR=0xF0000000` for `PLATFORM=cix-p1`
  (`linker.ld` already takes `HV_LOAD_ADDR`); the EL2 map follows
  `HAVEN_LOAD_PA`.
- qemu-virt: the preprocessed tables and code paths are kept equal, so
  its three images stay byte-identical; log strings are built from the
  same text.
- **Host test** `tests/unit/test_partition_layout.c`, built twice: with
  the qemu-virt layout, and with `HAVEN_PLATFORM_CIX_P1` and
  `HAVEN_PART_A_DTB_IPA=0x110000000`. It compiles `partition.c` against
  recording stubs (stage-2, SMMU, IRQ ownership, budgets, printk) and
  checks what `partitions_launch` asks for. Also in `scripts/dev/test.sh`.
- **CI:** one more compile-only step, `Cross-compile (cix-p1 draft, Linux
  layout, no boot)`, with `-DHAVEN_PART_A_DTB_IPA=0x110000000
  -DHAVEN_PART_A_VCPUS=2`.

### Expectations

Image comparisons build both trees with `SOURCE_DATE_EPOCH=1793318400`
exported (Step 1): "before" is `main` at this record's merge, "after" is
`main` at the code's merge. cix-linux now means `arm64-cix-p1` with
`-DHAVEN_PART_A_DTB_IPA=0x110000000 -DHAVEN_PART_A_VCPUS=2`.

| Row | Check | Expected result |
| - | - | - |
| P1 | qemu, psci, qemu-linux | `haven.bin` byte-identical before and after; sizes as the baseline table |
| P2 | cix (preset, demo guest) | builds, no compiler warning (the known linker warnings only); text smaller than D4's 43,196 by 100 to 2,500 bytes (no DMA grant, no partition B code); data 144; bss within 64 bytes of 5,251,064 |
| P3 | cix-linux | builds; text smaller than 45,784; bss within 64 bytes of 5,258,048 |
| P4 | cix with D4's option `-DHAVEN_PART_A_DTB_IPA=0x50000000` | compile fails on the static assertion that the device tree IPA lies in partition A's RAM |
| P5 | `nm -S` of `haven.elf` | `_start` at 0xf0000000 (cix, cix-linux) and 0x80000000 (qemu); `part_a_regions` 0x40 bytes (2 regions) on cix, 0x60 (3) on qemu; `part_a_irqs` 0x24 bytes (3 routes) on cix-linux and qemu-linux; no `part_b_irqs` and no `partition_b_secondary_cpu_init` on cix |
| P6 | `test_partition_layout` (qemu-virt layout) | partition 1 mapped once with 3 regions: 0x4000_0000 to PA 0x8080_0000 + 512 MiB RWX, 0x0900_0000 + 4 KiB Device RW, 0x7000_0000 to PA 0xA4C0_0000 + 2 MiB read-only; partition 2 once with 1 region: 0x6000_0000 to PA 0xA080_0000 + 64 MiB RWX; StreamID 8 assigned to partition 1 with window IOVA 0x5F00_0000 + 1 MiB to PA 0x9F80_0000; 2 MMIO regions added; IRQ routes in order (33, A, CPU 0), (27, B, CPU 2); 2 budgets |
| P7 | `test_partition_layout_cix` | partition 1 mapped once with 2 regions: 0x1_0000_0000 = PA, 2 GiB RWX, and 0x040D_0000 + 4 KiB Device RW; partition 2 never mapped; no SMMU call; IRQ routes in order (330, A, CPU 0), (27, A, CPU 0); 1 budget (A, 8 of 10 ms); the log has `partition B not started: no memory on this platform` |
| P8 | host CTest | 29 of 29 (2 new); `scripts/dev/test.sh` passes |
| P9 | Python tests | 53, all pass |
| P10 | trusted code, qemu | 66 files; 7,470 to 7,500 code lines (`partition.c` and qemu-virt `memory.h` only) |
| P11 | trusted code, cix and cix-linux | 66 files (qemu-virt `memory.h` no longer in the build); 7,455 to 7,495 code lines |
| P12 | CI on the code pull request | every job green, including the new cix-p1 Linux layout step |

Verdict rule as Step 1. What this step will not show: that the cix-p1
image runs (no board, no loader yet), anything about 8 vCPUs, or that the
RAM window is free on the 16 GB board.

### Results (2026-11-12, macOS host and CI)

Expectations merged first (haven `de1ebee`, pull request 66); the code
after that (commit `3473290`, merged as `45b6277`, pull request 69, CI
green on every job). "Before" built at `de1ebee`, "after" from the code
commit's tree, both with `SOURCE_DATE_EPOCH=1793318400`. Correction to the
baseline paragraph [F]: the macOS host's cross compiler, picked by
`cmake/arm64.cmake`, is Homebrew's `aarch64-linux-gnu-gcc` GCC 15.2.0, not
`aarch64-unknown-linux-gnu` (only `size` and `nm` came from the latter).

| Row | Observed | Verdict |
| - | - | - |
| P1 | qemu `b8b4ea32...`, psci `ceffd618...`, qemu-linux `c2308a15...` before and after (`cmp` equal; the same hashes D4 recorded); sizes as the baseline | as expected |
| P2 | builds, no compiler warning; text 41,712 (1,484 less than 43,196), data 144, bss 5,251,064 | as expected |
| P3 | builds; text 44,312, data 144, bss 5,258,048 | as expected |
| P4 | `error: static assertion failed: "the device tree IPA must lie in partition A's RAM"` (partition.c) | as expected |
| P5 | `_start` 0xf0000000 (cix, cix-linux), 0x80000000 (qemu); `part_a_regions` 0x40 (cix, cix-linux) and 0x60 (qemu, qemu-linux); `part_a_irqs` 0x24 on cix-linux and qemu-linux; cix has no `part_b_irqs` and no `partition_b_secondary_cpu_init` (qemu: 0xc and 0x8c bytes) | as expected |
| P6 | `test_partition_layout`: passes, every check of the row | as expected |
| P7 | `test_partition_layout_cix`: passes, every check of the row | as expected |
| P8 | local CTest 29 of 29, `scripts/dev/test.sh` passes; CI 29 of 29 on Linux, macOS and Windows | as expected |
| P9 | 53, OK | as expected |
| P10 | 66 files, **7,517** code lines (+62: `partition.c` 571 to 628, qemu-virt `memory.h` 38 to 43) | **prediction error** (7,470 to 7,500 predicted) |
| P11 | 66 files, **7,506** code lines (cix and cix-linux) | **prediction error** (7,455 to 7,495 predicted) |
| P12 | pull request 69: every job green, including `Cross-compile (cix-p1 draft, Linux layout, no boot)` | as expected |

**Verdict.** 12 rows: 10 hold, 2 are prediction errors (P10, P11) with one
cause: `partition.c` grew by 57 code lines, not the 15 to 45 assumed. The
estimate counted the new definitions and checks but not the
preprocessor lines of every `#ifdef`, `#else` and `#endif` (cloc counts
them as code), the no-DMA stub, the second budget branch, or
`clang-format` splitting the UART route over three lines (the
pre-commit hook reformatted the file once; the images were rebuilt
afterwards, with the same hashes). Nothing else differs.

What this shows [O]: a `PLATFORM=cix-p1` image now carries D3's partition
A layout (RAM, UART2, SPI 330, emulated GIC at the GIC-700 addresses),
no partition B, and links at 0xF000_0000; the qemu-virt images are
unchanged. What it does not show: that the image runs on the board, 8
vCPUs (the next step, `multi-vcpu.md` step 4), or that the RAM window
is free on the 16 GB board.

## Step 3: read-back reads the image's widths

### Problem

[F] `tools/readback/readback.py` assumes two sizes of HAVEN's memory:
`read_vgic_enables` reads the emulated GIC's enable bits for INTIDs 0 to
63 (two words), and `read_vcpus` reads partition A's vCPU table with at
most 4 entries and `state` at offset 40. On cix-p1 the model has 352 IDs
(D4), so INTID 330 is invisible; the vCPU table's size (`HV_PSCI_MAX_VCPUS`,
4 today) will grow with the 8-vCPU step. The owner approved widening
both (2026-11-09).

### Design

- HAVEN states both widths in its image, as read-only constants next to
  the code that defines them: `const hv_u32 hv_vgic_nr_irqs =
  HV_VGIC_NR_IRQS;` in `vgic.c` and `const hv_u32 hv_psci_max_vcpus =
  HV_PSCI_MAX_VCPUS;` in `psci.c`, declared in `vgic.h` and `psci.h`.
  `psci.c`'s offset assertion becomes the layout read-back uses,
  `offsetof(state) == 8 + 8 * HV_PSCI_MAX_VCPUS` (40 for 4 entries, as
  today).
- `readback.py` (v14): `vgic_width()` reads `hv_vgic_nr_irqs` from the
  dumped image; without the symbol (images before this change) it is 64,
  so old behaviour is kept. A value that is not a multiple of 32 from 64
  to 1024 stops read-back (`SystemExit`, fail closed).
  `read_vgic_enables` reads `width / 32` words. `read_vcpus` reads
  `hv_psci_max_vcpus` (4 without the symbol; outside 1 to 16 stops
  read-back), takes `state` at `8 + 8 * max` and caps the count at `max`.
- Tests: `tools/readback/test_readback_width.py`, with a fake image and a
  `.nm` listing (no QEMU, no cross tools): width 64 without the symbol;
  352 with it and INTID 330 visible; an invalid width stops read-back;
  the vCPU table with and without `hv_psci_max_vcpus` (offset 40 and
  count capped at 4; offset 72 and 8 entries with max 8); an invalid max
  stops read-back.
- No QEMU read-back run: the change keeps QEMU's behaviour by
  construction (64 and 4 are what the qemu-virt image states) and the unit
  tests check both paths.

### Expectations

| Row | Check | Expected result |
| - | - | - |
| W1 | sizes of the five variants (Step 2 commands) | text 8 to 16 bytes larger than after Step 2 (two 4-byte constants, alignment); data and bss unchanged; images not byte-identical to Step 2's (expected) |
| W2 | `nm -S` and the image | `hv_vgic_nr_irqs` (4 bytes) holds 64 on qemu-virt and 352 on cix-p1; `hv_psci_max_vcpus` (4 bytes) holds 4 on all five |
| W3 | Python tests | 53 existing pass unchanged, 6 new pass: 59 |
| W4 | host CTest | 29 of 29 |
| W5 | trusted code | qemu 66 files, 7,520 to 7,527 code lines; cix 66 files, 7,509 to 7,516 |
| W6 | CI on the code pull request | every job green |

### Results (2026-11-12, macOS host and CI)

Expectations merged first (haven `5052034`, pull request 70); the code
after that (commit `d3f2216`, merged as `49ab922`, pull request 71, CI
green on every job). Images built with `SOURCE_DATE_EPOCH=1793318400`.

| Row | Observed | Verdict |
| - | - | - |
| W1 | text +8 bytes on all five (qemu 43,148, psci 43,148, qemu-linux 45,724, cix 41,720, cix-linux 44,320); data 144 and bss as after Step 2 | as expected |
| W2 | `hv_vgic_nr_irqs`, 4 bytes: 0x40 (64) on qemu, psci, qemu-linux; 0x160 (352) on cix, cix-linux; `hv_psci_max_vcpus`, 4 bytes: 4 on all five (`objdump -s` of `haven.elf`) | as expected |
| W3 | local 59 tests, OK, none skipped; CI 59 run, 12 skipped (the dts-based tests, no `dtc` there, as before) | as expected |
| W4 | CTest 29 of 29 locally and in CI | as expected |
| W5 | qemu 66 files, 7,522 code lines; cix 66 files, 7,511 | as expected |
| W6 | pull request 71: every job green | as expected |

**Verdict.** 6 of 6 rows hold. Read-back now sees every INTID of the
emulated GIC the image has (INTID 330 on cix-p1) and the vCPU table at
the image's size. Not shown: a read-back run on QEMU (none was made; the
qemu-virt image states 64 and 4, the values read-back assumed before, and
the unit tests check both paths) or on the board, where read-back's QEMU
parts (`IMG_PA` 0x8000_0000, QEMU's GIC addresses and `NCPUS` = 4) do not
apply yet.

## Summary of D5 and next steps

- Step 1: images are reproducible by default (11 of 11 rows).
- Step 2: the cix-p1 image carries D3's partition A layout, no partition
  B, and links at 0xF000_0000; qemu-virt images byte-identical (10 of 12
  rows; 2 trusted-code count errors).
- Step 3: read-back takes the vGIC width and vCPU table size from the
  image (6 of 6 rows).
- Trusted code over D5: qemu-virt 7,455 to 7,522 code lines (+67), cix-p1
  67 files and 7,479 lines to 66 files and 7,511 (+32).
- Next [P]: 8 vCPUs for partition A (`multi-vcpu.md` step 4; it needs a
  larger `HV_PSCI_MAX_VCPUS` and `HV_MMIO_MAX_REGIONS` on cix-p1, and
  generalising the vCPU 1 code); partition B's own record; read-back's
  board path (image address and GIC from the platform).
