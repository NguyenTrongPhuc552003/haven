#!/usr/bin/env sh
set -eu

CC_BIN="${CC:-cc}"

echo "[test] running unit tests"
mkdir -p build/tests

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_core_stubs.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/test_core_stubs

./build/tests/test_core_stubs

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_smmu_dma.c \
	src/core/mm/stage2.c \
	src/core/dma/smmu.c \
	-o build/tests/test_smmu_dma

./build/tests/test_smmu_dma

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_el2_exceptions.c \
	src/core/exc/el2_exceptions.c \
	-o build/tests/test_el2_exceptions

./build/tests/test_el2_exceptions

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_timer.c \
	src/core/time/timer.c \
	-o build/tests/test_timer

./build/tests/test_timer

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_iommu_policy.c \
	src/core/iommu/iommu_policy.c \
	-o build/tests/test_iommu_policy

./build/tests/test_iommu_policy

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_guest_uart.c \
	src/guest/drivers/uart.c \
	-o build/tests/test_guest_uart

./build/tests/test_guest_uart

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_freertos_integration.c \
	tests/common/printk_stub.c \
	src/guest/rtos/freertos_integration.c \
	-o build/tests/test_freertos_integration

./build/tests/test_freertos_integration

echo "[test] running integration isolation flow"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_isolation_flow.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	src/core/dma/smmu.c \
	src/core/time/timer.c \
	src/core/iommu/iommu_policy.c \
	src/core/exc/el2_exceptions.c \
	-o build/tests/test_isolation_flow

./build/tests/test_isolation_flow

echo "[test] running integration negative isolation flow"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_isolation_negative.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	src/core/dma/smmu.c \
	src/core/exc/el2_exceptions.c \
	src/core/time/timer.c \
	-o build/tests/test_isolation_negative

./build/tests/test_isolation_negative

echo "[test] running fault-injection matrix"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_fault_injection.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	src/core/dma/smmu.c \
	src/core/time/timer.c \
	src/core/iommu/iommu_policy.c \
	src/core/exc/el2_exceptions.c \
	-o build/tests/test_fault_injection

./build/tests/test_fault_injection

echo "[test] running SMMU hardware integration test"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_smmu_hardware.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/dma/smmu.c \
	-o build/tests/test_smmu_hardware

./build/tests/test_smmu_hardware

echo "[test] running spatial isolation boundary tests"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/isolation/test_spatial_isolation.c \
	src/core/mm/stage2.c \
	src/core/iommu/iommu_policy.c \
	src/core/dma/smmu.c \
	-o build/tests/test_spatial_isolation

./build/tests/test_spatial_isolation

echo "[test] running temporal isolation boundary tests"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/isolation/test_temporal_isolation.c \
	tests/common/printk_stub.c \
	src/core/sched/budget.c \
	src/core/irq/ownership.c \
	src/core/time/timer.c \
	-o build/tests/test_temporal_isolation

./build/tests/test_temporal_isolation

echo "[test] running hypervisor invariant self-tests"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/selftests/test_hypervisor_invariants.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	src/core/time/timer.c \
	src/core/iommu/iommu_policy.c \
	src/core/dma/smmu.c \
	src/core/exc/el2_exceptions.c \
	-o build/tests/test_hypervisor_invariants

./build/tests/test_hypervisor_invariants

echo "[test] running two-partition isolation demo"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/demos/demo_two_partition.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	src/core/dma/smmu.c \
	src/core/time/timer.c \
	src/core/iommu/iommu_policy.c \
	src/core/exc/el2_exceptions.c \
	-o build/tests/demo_two_partition
./build/tests/demo_two_partition

echo "[test] running isolation latency benchmark"
mkdir -p build/benchmarks
"$CC_BIN" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude \
	tests/benchmarks/bench_isolation_latency.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	src/core/dma/smmu.c \
	src/core/time/timer.c \
	src/core/iommu/iommu_policy.c \
	-o build/tests/bench_isolation_latency
./build/tests/bench_isolation_latency

echo "[test] running temporal isolation benchmark"
"$CC_BIN" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude \
	tests/benchmarks/bench_temporal_isolation.c \
	tests/common/printk_stub.c \
	src/core/sched/budget.c \
	src/core/time/timer.c \
	-o build/tests/bench_temporal_isolation
./build/tests/bench_temporal_isolation

echo "[test] running stage-2 fault containment benchmark"
"$CC_BIN" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude \
	tests/benchmarks/bench_stage2_fault.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/bench_stage2_fault
./build/tests/bench_stage2_fault

echo "[test] running SMMU DMA policy benchmark"
"$CC_BIN" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude \
	tests/benchmarks/bench_smmu_policy.c \
	tests/common/printk_stub.c \
	src/core/dma/smmu.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/bench_smmu_policy
./build/tests/bench_smmu_policy

echo "[test] running secondary CPU isolation policy test"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_secondary_cpu_isolation.c \
	tests/common/printk_stub.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/test_secondary_cpu_isolation

./build/tests/test_secondary_cpu_isolation

echo "[test] validating configuration schema"
./scripts/compile/check-configs.sh

echo "[test] done"
