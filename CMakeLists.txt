cmake_minimum_required(VERSION 3.20)
project(haven C ASM)

# -------------------------------------------------------------------------
# Haven CMakeLists.txt - IDE support only (CLion, VS Code CMake Tools)
# Primary build system: Makefile
# Usage: cmake -S . -B build-cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm64.cmake
# -------------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)

option(HAVEN_ARCH_ARM64 "Build for ARM64 bare-metal" OFF)

set(HAVEN_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/arch/arm64/include
    ${CMAKE_SOURCE_DIR}
)

# Core policy sources (always visible for IDE navigation)
set(HAVEN_CORE_SRCS
    src/core/mm/stage2.c
    src/core/irq/ownership.c
    src/core/sched/budget.c
    src/core/dma/smmu.c
    src/core/iommu/iommu_policy.c
    src/core/time/timer.c
    src/core/exc/el2_exceptions.c
    src/core/partition.c
    src/common/printk.c
    src/common/string.c
    src/common/panic.c
)

if(HAVEN_ARCH_ARM64)
    set(HAVEN_ARCH_SRCS
        arch/arm64/cpu.c
        arch/arm64/mm.c
        arch/arm64/timer.c
        arch/arm64/irq.c
        src/core/init.c
        src/common/spinlock.c
        src/platform/qemu-virt/platform.c
        drivers/irqchip/gic_v3.c
        drivers/iommu/smmu_v3.c
        drivers/uart/pl011.c
    )
    add_compile_definitions(HAVEN_ARCH_ARM64)
    add_compile_options(-ffreestanding -fno-builtin -nostdlib -march=armv8-a -mgeneral-regs-only)
endif()

add_library(haven_core STATIC ${HAVEN_CORE_SRCS} ${HAVEN_ARCH_SRCS})
target_include_directories(haven_core PUBLIC ${HAVEN_INCLUDES})
target_compile_options(haven_core PRIVATE -Wall -Wextra)
