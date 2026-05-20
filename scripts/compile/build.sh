#!/usr/bin/env sh
set -eu

CC_BIN="${CC:-cc}"

echo "[build] compiling haven core stubs"
mkdir -p build/obj

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/mm/stage2.c -o build/obj/stage2.o
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/irq/ownership.c -o build/obj/ownership.o
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/sched/budget.c -o build/obj/budget.o
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/dma/smmu.c -o build/obj/smmu.o
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/core/exc/el2_exceptions.c -o build/obj/el2_exceptions.o

echo "[build] compiling haven guest drivers"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/guest/drivers/uart.c -o build/obj/uart.o

echo "[build] compiling haven RTOS integration"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -c src/guest/rtos/freertos_integration.c -o build/obj/freertos_integration.o

ar rcs build/libhaven_core.a \
	build/obj/stage2.o \
	build/obj/ownership.o \
	build/obj/budget.o \
	build/obj/smmu.o \
	build/obj/el2_exceptions.o \
	build/obj/uart.o \
	build/obj/freertos_integration.o

echo "[build] produced build/libhaven_core.a"
