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

echo "[test] running integration isolation flow"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
	tests/integration/test_isolation_flow.c \
	src/core/mm/stage2.c \
	src/core/irq/ownership.c \
	src/core/sched/budget.c \
	-o build/tests/test_isolation_flow

./build/tests/test_isolation_flow

echo "[test] validating configuration schema"
./scripts/check-configs.sh

echo "[test] done"
