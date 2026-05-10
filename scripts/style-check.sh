#!/usr/bin/env sh
set -eu

echo "[style] checking shell scripts with sh -n"
find scripts -type f -name "*.sh" -exec sh -n {} \;

echo "[style] checking core C sources with cc -fsyntax-only"
cc -std=c11 -Wall -Wextra -Werror -Iinclude -fsyntax-only src/core/mm/stage2.c
cc -std=c11 -Wall -Wextra -Werror -Iinclude -fsyntax-only src/core/irq/ownership.c
cc -std=c11 -Wall -Wextra -Werror -Iinclude -fsyntax-only src/core/sched/budget.c

echo "[style] done"
