# Haven Tools - Scripts

Helper shell scripts for building Haven and launching QEMU guests.

---

## `cross-compile.sh`

Detects or installs the AArch64 cross-compilation toolchain and runs a full
`make ARCH=arm64` build of the Haven hypervisor.

### Usage

```bash
tools/scripts/cross-compile.sh [--toolchain PATH] [--jobs N]
```

### Behaviour

1. **Toolchain detection** - checks for `aarch64-unknown-linux-gnu-gcc` or
   `aarch64-linux-gnu-gcc` on `$PATH`.  If neither is found, it attempts
   automatic installation via `apt-get` (Debian/Ubuntu) or `brew`
   (macOS/Homebrew).
2. **Build invocation** - runs:
   ```bash
   make ARCH=arm64 CROSS_COMPILE=<detected-prefix> -j${JOBS:-$(nproc)} all
   ```
3. **Artefacts** - places the hypervisor ELF at `build/haven.elf` and a flat
   binary at `build/haven.bin`.

### Options

| Flag          | Default       | Description                                                   |
| ------------- | ------------- | ------------------------------------------------------------- |
| `--toolchain` | auto-detected | Override: prefix path, e.g. `/opt/xcc/bin/aarch64-linux-gnu-` |
| `--jobs`      | `nproc`       | Parallel make jobs                                            |

### Prerequisites

| Dependency                      | Minimum version | Notes                         |
| ------------------------------- | --------------- | ----------------------------- |
| `aarch64-unknown-linux-gnu-gcc` | GCC 12+         | Or any AArch64 cross-compiler |
| GNU Make                        | 4.3+            |                               |
| Python 3                        | 3.10+           | Used by config-gen scripts    |

---

## `qemu-run.sh`

Thin wrapper around `scripts/qemu-run.sh` that sets sensible defaults and
validates that QEMU is new enough before launching.

### Usage

```bash
tools/scripts/qemu-run.sh [--config CONFIG_YAML] [--debug]
```

### Behaviour

1. **QEMU version check** - parses `qemu-system-aarch64 --version` and aborts
   if the detected version is older than 5.0.
2. **Config loading** - reads `CONFIG_YAML` (default: `configs/arm64/qemu-virt.yaml`)
   to determine machine type, memory size, partition ELF images, and device
   passthrough settings.
3. **QEMU invocation** - delegates to `scripts/qemu-run.sh` with the resolved
   flags.

### Options

| Flag       | Default                        | Description                                  |
| ---------- | ------------------------------ | -------------------------------------------- |
| `--config` | `configs/arm64/qemu-virt.yaml` | YAML config file for the virtual platform    |
| `--debug`  | off                            | Adds `-s -S` to enable GDB stub on port 1234 |

### Prerequisites

| Dependency                      | Minimum version | Notes                                          |
| ------------------------------- | --------------- | ---------------------------------------------- |
| `qemu-system-aarch64`           | QEMU 5.0+       | Earlier versions lack required GICv3 emulation |
| `aarch64-unknown-linux-gnu-gcc` | GCC 12+         | Only needed if ELF images are not pre-built    |

---

## Notes

- Both scripts are idempotent: running them multiple times is safe.
- For CI, `cross-compile.sh` is invoked by `.github/workflows/ci.yml`; manual
  use is intended for local development on macOS or non-standard Linux setups.
