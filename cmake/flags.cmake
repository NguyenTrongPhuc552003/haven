# Haven compiler flag definitions
# Included by root CMakeLists.txt; not a standalone toolchain file.

# ── Common flags (host and ARM64) ─────────────────────────────────────────────
set(HAVEN_COMMON_CFLAGS -std=c11 -Wall -Wextra -Werror)

# ── ARM64 bare-metal C flags ──────────────────────────────────────────────────
set(HAVEN_ARM64_CFLAGS
    ${HAVEN_COMMON_CFLAGS}
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-pie
    -nostdlib
    -march=armv8-a
    -mgeneral-regs-only
)

# ── ARM64 assembler flags (applied per-.S file) ───────────────────────────────
# Note: these are joined as a space-separated string for COMPILE_FLAGS property.
set(HAVEN_ARM64_ASFLAGS "-D__ASSEMBLY__ -x assembler-with-cpp")

# ── ARM64 linker flags ────────────────────────────────────────────────────────
set(HAVEN_ARM64_LDFLAGS
    -ffreestanding
    -nostdlib
    -static
)

# ── Host (native) C flags ─────────────────────────────────────────────────────
set(HAVEN_HOST_CFLAGS ${HAVEN_COMMON_CFLAGS})

# ── Benchmark extra define ────────────────────────────────────────────────────
set(HAVEN_BENCH_DEFINES _POSIX_C_SOURCE=200809L)
