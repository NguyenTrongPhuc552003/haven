#!/usr/bin/env sh
set -eu

CC_BIN="${CC:-cc}"

echo "[build] compiling haven core stubs"
mkdir -p build/obj

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/mm/stage2.c -o build/obj/stage2.o
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/irq/ownership.c -o build/obj/ownership.o
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/sched/budget.c -o build/obj/budget.o
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/dma/smmu.c -o build/obj/smmu.o

ar rcs build/libhaven_core.a \
	build/obj/stage2.o \
	build/obj/ownership.o \
	build/obj/budget.o \
	build/obj/smmu.o

echo "[build] produced build/libhaven_core.a"
