#!/usr/bin/env sh
set -eu

CC_BIN="${CC:-cc}"

echo "[style] checking shell scripts with bash -n"
find scripts -type f -name "*.sh" -exec bash -n {} \;

echo "[style] checking core C sources with cc -fsyntax-only"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -fsyntax-only src/core/mm/stage2.c
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -fsyntax-only src/core/irq/ownership.c
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude -fsyntax-only src/core/sched/budget.c

echo "[style] done"
