#!/usr/bin/env bash
# Haven cross-compile environment setup and validation.
# Detects or installs the aarch64-unknown-linux-gnu toolchain.
set -euo pipefail

PREFERRED="aarch64-unknown-linux-gnu-"
FALLBACK="aarch64-elf-"
LINUX_FALLBACK="aarch64-linux-gnu-"
TOOLCHAIN_OVERRIDE=""
JOBS=""

check_toolchain() {
    local prefix="$1"
    if command -v "${prefix}gcc" &>/dev/null; then
        local ver
        ver=$("${prefix}gcc" --version | head -1)
        echo "[cross-compile] Found: ${prefix}gcc - $ver"
        return 0
    fi
    return 1
}

install_macos() {
    echo "[cross-compile] Installing aarch64-unknown-linux-gnu via brew..."
    brew tap messense/macos-cross-toolchains
    brew install aarch64-unknown-linux-gnu
}

install_linux() {
    echo "[cross-compile] Installing aarch64-linux-gnu-gcc via apt..."
    sudo apt-get install -y gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --toolchain)
            shift
            TOOLCHAIN_OVERRIDE="${1:-}"
            ;;
        --jobs)
            shift
            JOBS="${1:-}"
            ;;
        *)
            echo "[cross-compile] ERROR: Unknown argument: $1"
            echo "Usage: $0 [--toolchain PREFIX] [--jobs N]"
            exit 1
            ;;
    esac
    shift
done

echo "[cross-compile] Haven cross-compile environment check"

if [[ -n "$TOOLCHAIN_OVERRIDE" ]]; then
    if ! check_toolchain "$TOOLCHAIN_OVERRIDE"; then
        echo "[cross-compile] ERROR: --toolchain prefix not found: ${TOOLCHAIN_OVERRIDE}gcc"
        exit 1
    fi
    CROSS_COMPILE="$TOOLCHAIN_OVERRIDE"
elif check_toolchain "$PREFERRED"; then
    CROSS_COMPILE="$PREFERRED"
elif check_toolchain "$FALLBACK"; then
    CROSS_COMPILE="$FALLBACK"
    echo "[cross-compile] WARNING: Using fallback $FALLBACK. Prefer $PREFERRED."
elif check_toolchain "$LINUX_FALLBACK"; then
    CROSS_COMPILE="$LINUX_FALLBACK"
    echo "[cross-compile] WARNING: Using fallback $LINUX_FALLBACK. Prefer $PREFERRED."
else
    echo "[cross-compile] No AArch64 cross-compiler found. Attempting install..."
    if [[ "$(uname)" == "Darwin" ]]; then
        install_macos
    elif [[ "$(uname)" == "Linux" ]]; then
        install_linux
    else
        echo "[cross-compile] ERROR: Unsupported OS. Install manually."
        exit 1
    fi
    if check_toolchain "$PREFERRED"; then
        CROSS_COMPILE="$PREFERRED"
    elif check_toolchain "$LINUX_FALLBACK"; then
        CROSS_COMPILE="$LINUX_FALLBACK"
    else
        echo "[cross-compile] ERROR: Toolchain install completed but compiler still missing."
        exit 1
    fi
fi

echo "[cross-compile] Building with cmake --preset arm64-qemu (toolchain auto-detected)..."
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ -z "$JOBS" ]]; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
fi
pushd "$ROOT" >/dev/null
cmake --preset arm64-qemu
cmake --build build --parallel "$JOBS"
popd >/dev/null
echo "[cross-compile] SUCCESS: build/haven.elf produced."
