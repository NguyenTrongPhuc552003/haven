# Haven Hypervisor - Reproducible Build Container
#
# Provides a fully pinned environment for ARM64 cross-compilation, host
# unit-test execution, and thesis evidence generation.
#
# Build:  docker build -t haven-builder .
# Shell:  docker run --rm -it -v $(pwd):/haven haven-builder bash
# Build:  docker run --rm -v $(pwd):/haven haven-builder make ARCH=arm64 all
# Tests:  docker run --rm -v $(pwd):/haven haven-builder make test

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Host build tools
    gcc \
    clang \
    make \
    # ARM64 cross-compiler (aarch64-linux-gnu-gcc)
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    # ARM64 bare-metal cross-compiler (aarch64-none-elf-gcc)
    gcc-arm-none-eabi \
    # QEMU for virtual platform validation
    qemu-system-aarch64 \
    # Python for analysis scripts and config validation
    python3 \
    python3-pip \
    # Optional: Coq for formal proof verification
    coq \
    # Utilities
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Python analysis dependencies
RUN pip3 install --no-cache-dir matplotlib numpy

WORKDIR /haven

# Verify toolchain availability
RUN aarch64-linux-gnu-gcc --version && \
    qemu-system-aarch64 --version && \
    coqc --version

CMD ["bash"]
