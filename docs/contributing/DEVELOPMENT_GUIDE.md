# Development Guide

Haven is an EL2 static-partition hypervisor written in C11 (core TCB) with
supporting scripts and documentation. This guide covers everything needed to
build, run, and extend the project on macOS and Linux.

---

## Prerequisites

### Required tools (all platforms)

| Tool                      | Minimum version    | Notes                                       |
| ------------------------- | ------------------ | ------------------------------------------- |
| C compiler (cc/gcc/clang) | GCC 12 or Clang 15 | Must support C11                            |
| CMake                     | 3.22               | Primary build system; Presets v3 support    |
| Ninja                     | 1.10               | Default CMake generator                     |
| QEMU                      | 7.2                | `qemu-system-aarch64` for ARM64 smoke tests |
| Python                    | 3.10               | Evidence comparison script                  |

### ARM64 cross-compiler (for `cmake --preset arm64-qemu`)

#### macOS (Homebrew)

```bash
brew install aarch64-unknown-linux-gnu  # or: llvm for clang cross
export CROSS_COMPILE=aarch64-unknown-linux-gnu-
export CC=${CROSS_COMPILE}gcc
```

#### Ubuntu / Debian

```bash
sudo apt install gcc-aarch64-linux-gnu
export CROSS_COMPILE=aarch64-unknown-linux-gnu-
export CC=${CROSS_COMPILE}gcc
```

#### Fedora / RHEL

```bash
sudo dnf install gcc-aarch64-linux-gnu
export CROSS_COMPILE=aarch64-unknown-linux-gnu-
```

### QEMU installation

#### macOS

```bash
brew install qemu
```

#### Ubuntu / Debian

```bash
sudo apt install qemu-system-arm
```

---

## Build instructions

### Host tests (no cross-compiler needed)

Host tests compile and run on your development machine using the native
toolchain. They stub out EL2 hardware instructions so the policy logic can
be exercised on x86-64 or Apple Silicon without QEMU.

```bash
cmake --preset host-tests
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

This builds and runs 21 unit, integration, isolation, selftest, demo, and
benchmark targets. CTest fails on the first failing test unless
`--stop-on-failure` is omitted.

### ARM64 cross-compile (hypervisor image)

```bash
# Auto-detects aarch64-unknown-linux-gnu- or aarch64-elf-
cmake --preset arm64-qemu
cmake --build build
# Outputs: build/haven.elf, build/haven.bin, build/guest_a.bin, build/guest_b.bin
```

For the i.MX95 Dev Kit (thesis primary board):

```bash
cmake --preset arm64-imx95
cmake --build build-imx95
```

### QEMU smoke test

```bash
cmake --build build --target qemu-run
```

Or directly:

```bash
bash scripts/qemu/qemu-run.sh
```

The script boots `build/haven.bin` under `qemu-system-aarch64 -M virt` and
checks that the hypervisor prints its startup banner without faulting. It
exits 0 on success, 1 on failure.

---

## Development workflow

### Branch naming

```
<type>/<short-description>
```

Types: `feat`, `fix`, `bench`, `test`, `docs`, `refactor`, `chore`.

Examples:
- `feat/smmu-stream-table`
- `fix/budget-consume-overflow`
- `bench/stage2-boundary-cases`

### Commit style

```
<type>(<scope>): <imperative summary under 72 chars>

Optional body - explain *why*, not *what*.
```

Scope is optional but recommended for core subsystems:
`stage2`, `budget`, `smmu`, `timer`, `irq`, `iommu`, `exc`.

### Test-first approach

For every new enforcement path:

1. Write a unit test in `tests/unit/test_<subsystem>.c` that asserts the
   invariant before implementing it.
2. Implement the invariant in `src/core/<subsystem>/`.
3. Add a negative test to `tests/integration/test_isolation_negative.c`
   proving a cross-partition violation is rejected.
4. Run `ctest --test-dir build-host` — all tests must pass before opening a PR.

---

## Debugging

### QEMU GDB stub

Start QEMU and pause at reset:

```bash
qemu-system-aarch64 \
  -M virt -cpu cortex-a53 -m 512M -nographic \
  -kernel build/haven.elf \
  -S -gdb tcp::1234
```

Then in another terminal:

```bash
aarch64-unknown-linux-gnu-gdb build/haven.elf
(gdb) target remote :1234
(gdb) set architecture aarch64
(gdb) continue
```

### EL2 register inspection

Inside GDB, with QEMU:

```
(gdb) info registers
(gdb) p/x $hcr_el2     # Hypervisor Configuration Register
(gdb) p/x $vtcr_el2    # Virtualization Translation Control Register
(gdb) p/x $vttbr_el2   # Stage-2 Translation Table Base Register
```

QEMU exposes all EL2 system registers via the GDB remote protocol when
`-cpu cortex-a53` (or any ARMv8.0-A model) is selected.

### Tracing budget scheduler decisions

Add `-DHAVEN_TRACE_BUDGET=1` to `CFLAGS` to enable `printk`-level tracing
from `src/core/sched/budget.c`. Each `hv_budget_consume()` call prints the
remaining budget and partition ID to the debug UART.

```bash
cmake --preset arm64-qemu -DCMAKE_C_FLAGS="-DHAVEN_TRACE_BUDGET=1"
cmake --build build
```
