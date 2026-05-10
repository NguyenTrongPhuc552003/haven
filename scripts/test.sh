#!/usr/bin/env sh
set -eu

CC_BIN="${CC:-cc}"

echo "[test] running unit tests"
mkdir -p build/tests

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_core_stubs.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/test_core_stubs

./build/tests/test_core_stubs

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_smmu_dma.c \
	src/core/dma/smmu.c \
	-o build/tests/test_smmu_dma

./build/tests/test_smmu_dma

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_el2_exceptions.c \
	src/core/exc/el2_exceptions.c \
	-o build/tests/test_el2_exceptions

./build/tests/test_el2_exceptions

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/unit/test_guest_uart.c \
	src/guest/drivers/uart.c \
	-o build/tests/test_guest_uart

./build/tests/test_guest_uart

echo "[test] running integration isolation flow"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_isolation_flow.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/test_isolation_flow

./build/tests/test_isolation_flow

echo "[test] running integration negative isolation flow"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_isolation_negative.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/test_isolation_negative

./build/tests/test_isolation_negative

echo "[test] validating configuration schema"
./scripts/check-configs.sh

echo "[test] done"
