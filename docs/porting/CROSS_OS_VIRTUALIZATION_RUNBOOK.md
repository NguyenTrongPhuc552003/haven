# Cross-OS Virtualization Runbook

This runbook describes how to validate the project on macOS, Linux, and Windows.

## macOS path (OrbStack + QEMU)

1. Install OrbStack and confirm Docker-compatible CLI is available.
2. Validate container workflow:
- docker version
- docker run --rm alpine:latest uname -a
3. Install QEMU and run:
- qemu-system-aarch64 --version
4. Run project checks:
- ./scripts/ci-preflight.sh
- ./scripts/qemu-smoke.sh

## Linux path (Docker engine + QEMU)

1. Install Docker engine.
2. Validate container workflow:
- docker version
- docker run --rm alpine:latest uname -a
3. Install qemu-system-aarch64 package.
4. Run project checks:
- ./scripts/ci-preflight.sh
- ./scripts/qemu-smoke.sh

## Windows path (WSL2 or Docker Desktop + QEMU)

1. Prepare bash-capable environment (WSL2 recommended).
2. Install Docker Desktop or WSL2 Docker path.
3. Install QEMU for Windows or WSL2 distribution.
4. Run project checks in bash shell:
- ./scripts/ci-preflight.sh
- ./scripts/qemu-smoke.sh

## Evidence capture

1. Save host metadata and command transcript.
2. Capture pass/fail for each validation level.
3. Link evidence into milestone records.
