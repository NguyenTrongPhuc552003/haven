#!/usr/bin/env sh
# Build haven for ARM64. Detects installed cross-compiler prefix.
set -eu

JOBS=${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}

# Probe for available cross-compiler prefix
for PREFIX in aarch64-unknown-linux-gnu- aarch64-linux-gnu- aarch64-none-elf-; do
    if command -v "${PREFIX}gcc" >/dev/null 2>&1; then
        CROSS_COMPILE="$PREFIX"
        break
    fi
done

if [ -z "${CROSS_COMPILE:-}" ]; then
    echo "[build-arm64] ERROR: no aarch64 cross-compiler found"
    echo "  Install with: sudo apt-get install gcc-aarch64-linux-gnu"
    echo "  Or on macOS:  brew install aarch64-elf-gcc"
    exit 1
fi

echo "[build-arm64] using cross-compiler: ${CROSS_COMPILE}gcc"
echo "[build-arm64] jobs: ${JOBS}"

CROSS_COMPILE="${CROSS_COMPILE}" cmake --preset arm64-qemu
CROSS_COMPILE="${CROSS_COMPILE}" cmake --build build -- -j"${JOBS}"

echo "[build-arm64] build complete: build/haven.elf  build/haven.bin"
${CROSS_COMPILE}size build/haven.elf
