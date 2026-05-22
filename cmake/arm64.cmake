# Haven ARM64 cross-compile toolchain file
# Usage: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm64.cmake

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Auto-detect cross-compiler: prefer aarch64-unknown-linux-gnu-, fall back to aarch64-elf-
find_program(_CROSS_GCC_PROBE aarch64-unknown-linux-gnu-gcc)
if(_CROSS_GCC_PROBE)
    set(CROSS_PREFIX aarch64-unknown-linux-gnu-)
else()
    set(CROSS_PREFIX aarch64-elf-)
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

