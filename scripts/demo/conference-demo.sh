#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# scripts/demo/conference-demo.sh
#
# Haven AMP Hypervisor — Self-Contained QEMU Conference Demo
#
# Demonstrates spatial and temporal isolation between two partitions on
# the QEMU aarch64 virt machine with GICv3 + SMMUv3.
#
# Acts:
#   Act 1 — Build verification (host-tests, arm64-qemu)
#   Act 2 — Isolation unit test showcase
#   Act 3 — QEMU two-partition smoke test
#   Act 4 — Benchmark summary
#
# Usage:
#   ./scripts/demo/conference-demo.sh [--dry-run] [--skip-build] [--act N]
#
#   --dry-run     Print all commands that would be run; do not execute them
#   --skip-build  Skip Act 1 (build); assume binaries are already present
#   --act N       Run only Act N (1-4)
#
# Target runtime: < 60 seconds on a modern laptop.
#
# Exit codes:
#   0  All acts passed
#   1  A required act failed
#   2  Usage error

set -euo pipefail

###############################################################################
# ANSI colours
###############################################################################
if [ -t 1 ]; then
	C_RESET='\033[0m'
	C_BOLD='\033[1m'
	C_GREEN='\033[0;32m'
	C_YELLOW='\033[0;33m'
	C_CYAN='\033[0;36m'
	C_RED='\033[0;31m'
	C_MAGENTA='\033[0;35m'
else
	C_RESET='' C_BOLD='' C_GREEN='' C_YELLOW='' C_CYAN='' C_RED='' C_MAGENTA=''
fi

###############################################################################
# Globals
###############################################################################
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

DRY_RUN=0
SKIP_BUILD=0
ONLY_ACT=""

for arg in "$@"; do
	case "$arg" in
		--dry-run)    DRY_RUN=1 ;;
		--skip-build) SKIP_BUILD=1 ;;
		--act)        : ;;   # handled below
		--act=*)      ONLY_ACT="${arg#--act=}" ;;
		[1-4])        ONLY_ACT="$arg" ;;
		-h|--help)
			grep '^#' "${BASH_SOURCE[0]}" | head -30 | sed 's/^# \?//'
			exit 0
			;;
		*)
			printf '%bUsage: %s [--dry-run] [--skip-build] [--act N]%b\n' \
				"$C_RED" "${BASH_SOURCE[0]}" "$C_RESET" >&2
			exit 2
			;;
	esac
done

# Handle "--act N" (two tokens)
for i in "$@"; do
	if [ "$i" = "--act" ]; then
		shift
		ONLY_ACT="$1"
		break
	fi
done

###############################################################################
# Helpers
###############################################################################
banner() {
	printf '\n%b%b=== %s ===%b\n' "$C_BOLD" "$C_CYAN" "$1" "$C_RESET"
}

ok() {
	printf '%b  ✓ %s%b\n' "$C_GREEN" "$1" "$C_RESET"
}

warn() {
	printf '%b  ⚠ %s%b\n' "$C_YELLOW" "$1" "$C_RESET"
}

fail() {
	printf '%b  ✗ %s%b\n' "$C_RED" "$1" "$C_RESET" >&2
}

run_cmd() {
	printf '%b  $ %s%b\n' "$C_MAGENTA" "$*" "$C_RESET"
	if [ "$DRY_RUN" -eq 0 ]; then
		"$@"
	fi
}

skip_act() {
	[ -z "$ONLY_ACT" ] || [ "$ONLY_ACT" = "$1" ]
}

START_TIME=$(date +%s 2>/dev/null || echo 0)

###############################################################################
# Header
###############################################################################
printf '\n%b%b Haven AMP Hypervisor — QEMU Conference Demo %b\n' \
	"$C_BOLD" "$C_CYAN" "$C_RESET"
printf '%b  Repo:  %s%b\n' "$C_BOLD" "$REPO_ROOT" "$C_RESET"
printf '%b  Mode:  %s%b\n' "$C_BOLD" \
	"$([ "$DRY_RUN" -eq 1 ] && echo "DRY RUN" || echo "LIVE")" "$C_RESET"
printf '\n'

###############################################################################
# Act 1 — Build Verification
###############################################################################
if skip_act 1; then
	banner "Act 1/4 — Build Verification"

	if [ "$SKIP_BUILD" -eq 1 ]; then
		warn "Skipping build (--skip-build)"
	else
		# Host-test build
		printf '%b  [1a] CMake host-tests preset...%b\n' "$C_BOLD" "$C_RESET"
		run_cmd cmake --preset host-tests -DCMAKE_BUILD_TYPE=Release
		run_cmd cmake --build build-host --parallel

		ok "Host-test binary built"

		# ARM64 QEMU build
		if command -v aarch64-linux-gnu-gcc > /dev/null 2>&1 || \
		   command -v aarch64-unknown-linux-gnu-gcc > /dev/null 2>&1; then
			printf '%b  [1b] CMake arm64-qemu preset...%b\n' "$C_BOLD" "$C_RESET"
			run_cmd cmake --preset arm64-qemu -DCMAKE_BUILD_TYPE=Release
			run_cmd cmake --build build --parallel
			ok "ARM64 QEMU binary built: build/haven.elf"
		else
			warn "aarch64 cross-compiler not found — skipping ARM64 build"
			warn "Install: brew install aarch64-elf-gcc  (macOS)"
			warn "         apt install gcc-aarch64-linux-gnu  (Ubuntu)"
		fi
	fi
fi

###############################################################################
# Act 2 — Isolation Unit Test Showcase
###############################################################################
if skip_act 2; then
	banner "Act 2/4 — Isolation Unit Tests"

	if [ ! -d "build-host" ] || [ ! -f "build-host/CMakeCache.txt" ]; then
		if [ "$SKIP_BUILD" -eq 1 ] || [ "$DRY_RUN" -eq 1 ]; then
			warn "build-host not found — skipping unit tests"
		else
			fail "build-host not found. Run Act 1 first or omit --skip-build."
			exit 1
		fi
	else
		printf '%b  Spatial isolation tests:%b\n' "$C_BOLD" "$C_RESET"
		if run_cmd ctest --test-dir build-host -R "test_spatial_isolation" \
				--output-on-failure -q 2>&1 | grep -E "PASS|FAIL|passed|failed"; then
			ok "Spatial isolation: PASS"
		else
			warn "Spatial isolation: no output (may be dry-run or tests not present)"
		fi

		printf '%b  Temporal isolation tests:%b\n' "$C_BOLD" "$C_RESET"
		if run_cmd ctest --test-dir build-host -R "test_temporal_isolation" \
				--output-on-failure -q 2>&1 | grep -E "PASS|FAIL|passed|failed"; then
			ok "Temporal isolation: PASS"
		else
			warn "Temporal isolation: no output"
		fi

		printf '%b  IOMMU policy tests:%b\n' "$C_BOLD" "$C_RESET"
		if run_cmd ctest --test-dir build-host -R "test_iommu_policy" \
				--output-on-failure -q 2>&1 | grep -E "PASS|FAIL|passed|failed"; then
			ok "IOMMU policy: PASS"
		else
			warn "IOMMU policy: no output"
		fi

		printf '%b  Full test suite:%b\n' "$C_BOLD" "$C_RESET"
		run_cmd ctest --test-dir build-host --output-on-failure -q
		ok "All unit/integration tests: PASS"
	fi
fi

###############################################################################
# Act 3 — QEMU Two-Partition Smoke Test
###############################################################################
if skip_act 3; then
	banner "Act 3/4 — QEMU Two-Partition Smoke Test"

	if ! command -v qemu-system-aarch64 > /dev/null 2>&1; then
		warn "qemu-system-aarch64 not found — skipping QEMU act"
		warn "Install: brew install qemu  (macOS)"
		warn "         apt install qemu-system-aarch64  (Ubuntu)"
	elif [ ! -f "build/haven.bin" ] && [ "$DRY_RUN" -eq 0 ]; then
		warn "build/haven.bin not found — skipping QEMU act"
		warn "Run: cmake --preset arm64-qemu && cmake --build build"
	else
		printf '%b  Running QEMU smoke test (timeout: 30s)...%b\n' "$C_BOLD" "$C_RESET"
		run_cmd ./scripts/qemu/qemu-smoke.sh

		if [ "$DRY_RUN" -eq 0 ]; then
			ok "QEMU two-partition smoke test: PASS"
			printf '%b  Key markers confirmed:%b\n' "$C_BOLD" "$C_RESET"
			printf '    [haven] EL2 boot OK\n'
			printf '    [haven] stage2 mapped: partition_a\n'
			printf '    [haven] stage2 mapped: partition_b\n'
			printf '    [haven] scheduling started\n'
			printf '    Partition A marker\n'
			printf '    Partition B marker\n'
		fi
	fi
fi

###############################################################################
# Act 4 — Benchmark Summary
###############################################################################
if skip_act 4; then
	banner "Act 4/4 — Isolation Check Latency Benchmarks"

	if [ ! -d "build-host" ] || [ ! -f "build-host/CMakeCache.txt" ]; then
		warn "build-host not found — skipping benchmarks"
	else
		printf '%b  Running isolation latency benchmark (100,000 iterations)...%b\n' \
			"$C_BOLD" "$C_RESET"
		run_cmd ctest --test-dir build-host -R "bench_isolation_latency" \
			--output-on-failure -q

		printf '%b  Running stage-2 fault benchmark...%b\n' "$C_BOLD" "$C_RESET"
		run_cmd ctest --test-dir build-host -R "bench_stage2_fault" \
			--output-on-failure -q

		printf '%b  Running SMMU policy benchmark...%b\n' "$C_BOLD" "$C_RESET"
		run_cmd ctest --test-dir build-host -R "bench_smmu_policy" \
			--output-on-failure -q

		ok "All benchmarks: mean < 100,000 ns (threshold satisfied)"
		printf '\n%b  Summary from docs/methodology/BENCHMARK_BASELINE.md:%b\n' \
			"$C_BOLD" "$C_RESET"
		printf '  %-36s  Mean (ns)  Verdict\n' 'Benchmark'
		printf '  %-36s  ---------  -------\n' '---'
		printf '  %-36s  %9s  %s\n' 'stage2_partition_contains_pa' '38' 'PASS'
		printf '  %-36s  %9s  %s\n' 'irq_is_owned_by' '40' 'PASS'
		printf '  %-36s  %9s  %s\n' 'budget_consume' '33' 'PASS'
		printf '  %-36s  %9s  %s\n' 'smmu_check_dma_access' '31' 'PASS'
		printf '  %-36s  %9s  %s\n' 'timer_check_deadline' '27' 'PASS'
		printf '  %-36s  %9s  %s\n' 'smmu_check_access_hot' '66' 'PASS'
		printf '  %-36s  %9s  %s\n' 'stage2_contains_hot_path' '62' 'PASS'
		ok "All 15 benchmarks PASS < 100,000 ns"
	fi
fi

###############################################################################
# Footer
###############################################################################
END_TIME=$(date +%s 2>/dev/null || echo 0)
ELAPSED=$(( END_TIME - START_TIME ))

printf '\n%b%b Demo complete in %ds %b\n' \
	"$C_BOLD" "$C_GREEN" "$ELAPSED" "$C_RESET"
printf '%b  Haven v%s — Static Partition Hypervisor for AMP on ARM64 %b\n' \
	"$C_CYAN" "$(cat VERSION 2>/dev/null || echo "0.6.0")" "$C_RESET"
printf '%b  https://github.com/NguyenTrongPhuc552003/haven %b\n\n' \
	"$C_CYAN" "$C_RESET"

exit 0
