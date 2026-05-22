# Haven ARM64 cross-compile toolchain file
# Usage: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm64.cmake

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Auto-detect cross-compiler.
# Priority:
#   1. aarch64-linux-gnu-      — Ubuntu/Debian gcc-aarch64-linux-gnu package (CI default)
#   2. aarch64-unknown-linux-gnu- — custom Linaro / crosstool-ng toolchain
#   3. aarch64-elf-            — bare-metal newlib toolchain
find_program(_CROSS_GCC_PROBE_LINUXGNU  aarch64-linux-gnu-gcc)
find_program(_CROSS_GCC_PROBE_UNKNOWN   aarch64-unknown-linux-gnu-gcc)
find_program(_CROSS_GCC_PROBE_ELF       aarch64-elf-gcc)

if(_CROSS_GCC_PROBE_LINUXGNU)
    set(CROSS_PREFIX aarch64-linux-gnu-)
elseif(_CROSS_GCC_PROBE_UNKNOWN)
    set(CROSS_PREFIX aarch64-unknown-linux-gnu-)
elseif(_CROSS_GCC_PROBE_ELF)
    set(CROSS_PREFIX aarch64-elf-)
else()
    message(FATAL_ERROR
        "No AArch64 cross-compiler found. Install one of:\n"
        "  Ubuntu/Debian: sudo apt-get install gcc-aarch64-linux-gnu\n"
        "  Homebrew:      brew install aarch64-elf-gcc\n"
        "Or set CMAKE_C_COMPILER explicitly.")
endif()

set(CMAKE_C_COMPILER   ${CROSS_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${CROSS_PREFIX}gcc)
set(CMAKE_LINKER       ${CROSS_PREFIX}ld)
set(CMAKE_AR           ${CROSS_PREFIX}ar    CACHE FILEPATH "Archiver")
set(CMAKE_OBJCOPY      ${CROSS_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_OBJDUMP      ${CROSS_PREFIX}objdump CACHE FILEPATH "objdump")
set(CMAKE_SIZE         ${CROSS_PREFIX}size    CACHE FILEPATH "size")

# Bare-metal: suppress CMake's default link test (would fail without libc).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Sysroot isolation — do not search host paths.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Signal to CMakeLists.txt that this is an ARM64 bare-metal build.
set(HAVEN_ARCH_ARM64 ON CACHE BOOL "Build for ARM64 bare-metal" FORCE)

