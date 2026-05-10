# Virtual Platform Validation Matrix

This document defines validation and verification workflows for virtual environments
across macOS, Linux, and Windows.

## Objectives

1. Guarantee reproducible developer and CI workflows across three common host OSes.
2. Verify containerized build and test workflows.
3. Verify QEMU-based virtual validation for partitioning scenarios.

## Host OS matrix

macOS:
1. OrbStack path for containerized development.
2. QEMU user installation path for arm64 emulation.

Linux:
1. Native Docker engine path.
2. Native QEMU path.

Windows:
1. Docker Desktop or WSL2-backed container path.
2. QEMU path with documented shell/toolchain configuration.

## Validation levels

Level V1: Environment sanity
1. Toolchain available.
2. Scripts run without shell incompatibility.

Level V2: Repository preflight
1. build
2. style-check
3. test
4. config-check

Level V3: QEMU workflow sanity
1. qemu-system-aarch64 available.
2. qemu invocation contract validated.
3. artifact and log collection complete.

## Evidence required per host OS

1. Host metadata file.
2. Command transcript for V1/V2/V3.
3. Any deviations and mitigation notes.

## Release policy

1. Release 2: V1 and V2 complete on all three OSes.
2. Release 3: V3 complete on all three OSes.
3. Release 4: Full regression rerun before thesis freeze.
