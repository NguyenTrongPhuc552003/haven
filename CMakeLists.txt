cmake_minimum_required(VERSION 3.22)
project(haven VERSION 0.6.2 LANGUAGES C ASM)

# ─────────────────────────────────────────────────────────────────────────────
# Haven — Static Partition Hypervisor
# Primary build system: CMake (replaces all Makefiles).
#
# Quick-start:
#   cmake --preset arm64-qemu && cmake --build build   # ARM64 QEMU target
#   cmake --preset host-tests  && cmake --build build-host && ctest --test-dir build-host
#
# See CMakePresets.json for all available presets.
# ─────────────────────────────────────────────────────────────────────────────

include(cmake/flags.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build options ─────────────────────────────────────────────────────────────
option(HAVEN_ARCH_ARM64  "Build for ARM64 bare-metal"    OFF)
option(HAVEN_HOST_BUILD  "Build for host (unit tests)"   OFF)
set(PLATFORM "qemu-virt" CACHE STRING
    "Target platform: qemu-virt | imx95-devkit | imx8qm-mek")

# Derive preprocessor platform symbol, e.g. imx95-devkit → HAVEN_PLATFORM_IMX95_DEVKIT
string(TOUPPER  "${PLATFORM}" _plat_upper)
string(REPLACE  "-" "_" _plat_sym "${_plat_upper}")
set(HAVEN_PLATFORM_DEFINE "HAVEN_PLATFORM_${_plat_sym}")

# ── Include paths ─────────────────────────────────────────────────────────────
set(HAVEN_ARM64_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/arch/arm64/include
    ${CMAKE_SOURCE_DIR}
)
set(HAVEN_HOST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)

# ── Core sources (portable — compiled for both ARM64 and host builds) ─────────
set(HAVEN_CORE_SRCS
    src/core/mm/stage2.c
    src/core/irq/ownership.c
    src/core/sched/budget.c
    src/core/dma/smmu.c
    src/core/iommu/iommu_policy.c
    src/core/time/timer.c
    src/core/exc/el2_exceptions.c
    src/core/partition.c
    src/core/init.c
)
set(HAVEN_COMMON_SRCS
    src/common/printk.c
    src/common/string.c
    src/common/panic.c
    src/common/spinlock.c
)
set(HAVEN_GUEST_SRCS
    src/guest/drivers/uart.c
    src/guest/rtos/freertos_integration.c
)

# ═════════════════════════════════════════════════════════════════════════════
# ARM64 cross-compile path
# Produces: haven.elf, haven.bin, guest_a.elf, guest_a.bin, guest_b.elf, guest_b.bin
# ═════════════════════════════════════════════════════════════════════════════
if(HAVEN_ARCH_ARM64)

    # ── Architecture sources ──────────────────────────────────────────────────
    set(HAVEN_ARCH_C_SRCS
        arch/arm64/cpu.c
        arch/arm64/mm.c
        arch/arm64/timer.c
        arch/arm64/irq.c
    )
    set(HAVEN_ARCH_ASM_SRCS
        arch/arm64/entry.S
        arch/arm64/context.S
        arch/arm64/boot.S
        arch/arm64/partition.S
    )

    # ── Hardware driver sources ───────────────────────────────────────────────
    # UART is platform-specific to avoid duplicate uart_putchar/uart_init symbols.
    set(HAVEN_DRIVER_SRCS
        drivers/irqchip/gic_v3.c
        drivers/iommu/smmu_v3.c
    )
    if(PLATFORM STREQUAL "imx95-devkit")
        list(APPEND HAVEN_DRIVER_SRCS drivers/uart/imx_uart.c)
    else()
        list(APPEND HAVEN_DRIVER_SRCS drivers/uart/pl011.c)
    endif()

    # ── Platform sources ──────────────────────────────────────────────────────
    set(HAVEN_PLATFORM_SRCS src/platform/${PLATFORM}/platform.c)

    # ── Set ARM64 assembler flags per .S source file ──────────────────────────
    foreach(_asm IN LISTS HAVEN_ARCH_ASM_SRCS)
        set_source_files_properties(${_asm} PROPERTIES
            COMPILE_FLAGS
            "${HAVEN_ARM64_ASFLAGS} -Iarch/arm64/include -Iinclude"
        )
    endforeach()

    # ── haven executable ──────────────────────────────────────────────────────
    add_executable(haven
        ${HAVEN_ARCH_C_SRCS}
        ${HAVEN_ARCH_ASM_SRCS}
        ${HAVEN_DRIVER_SRCS}
        ${HAVEN_CORE_SRCS}
        ${HAVEN_COMMON_SRCS}
        ${HAVEN_PLATFORM_SRCS}
        ${HAVEN_GUEST_SRCS}
    )
    target_include_directories(haven PRIVATE ${HAVEN_ARM64_INCLUDES})
    target_compile_options(haven    PRIVATE ${HAVEN_ARM64_CFLAGS})
    target_compile_definitions(haven PRIVATE
        HAVEN_ARCH_ARM64
        ${HAVEN_PLATFORM_DEFINE}
    )
    target_link_options(haven PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker.ld
        ${HAVEN_ARM64_LDFLAGS}
    )
    set_target_properties(haven PROPERTIES
        OUTPUT_NAME "haven"
        SUFFIX      ".elf"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Print section sizes after every link
    if(CMAKE_SIZE)
        add_custom_command(TARGET haven POST_BUILD
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:haven>
            COMMENT "haven.elf section sizes"
        )
    endif()

    # haven.bin — raw binary for flashing / QEMU -kernel
    add_custom_command(TARGET haven POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:haven>
                ${CMAKE_BINARY_DIR}/haven.bin
        COMMENT "objcopy → haven.bin"
    )

    # ── Guest A — bare-metal EL1 stub at PA 0x80800000 ───────────────────────
    set_source_files_properties(tests/demos/guest_a_entry.S PROPERTIES
        COMPILE_FLAGS
        "${HAVEN_ARM64_ASFLAGS} -Iarch/arm64/include -Iinclude"
    )
    add_executable(guest_a tests/demos/guest_a_entry.S)
    target_include_directories(guest_a PRIVATE
        ${CMAKE_SOURCE_DIR}/arch/arm64/include
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_options(guest_a PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker-guest.ld
        -nostdlib -static
    )
    set_target_properties(guest_a PROPERTIES
        OUTPUT_NAME "guest_a"
        SUFFIX      ".elf"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_custom_command(TARGET guest_a POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:guest_a>
                ${CMAKE_BINARY_DIR}/guest_a.bin
        COMMENT "objcopy → guest_a.bin"
    )

    # ── Guest B — RTOS-class EL1 stub at PA 0xA0800000 ───────────────────────
    set_source_files_properties(tests/demos/guest_b_entry.S PROPERTIES
        COMPILE_FLAGS
        "${HAVEN_ARM64_ASFLAGS} -Iarch/arm64/include -Iinclude"
    )
    add_executable(guest_b tests/demos/guest_b_entry.S)
    target_include_directories(guest_b PRIVATE
        ${CMAKE_SOURCE_DIR}/arch/arm64/include
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_options(guest_b PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker-guest-b.ld
        -nostdlib -static
    )
    set_target_properties(guest_b PROPERTIES
        OUTPUT_NAME "guest_b"
        SUFFIX      ".elf"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_custom_command(TARGET guest_b POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:guest_b>
                ${CMAKE_BINARY_DIR}/guest_b.bin
        COMMENT "objcopy → guest_b.bin"
    )

    # ── ARM64 helper targets ──────────────────────────────────────────────────
    add_custom_target(qemu-run
        DEPENDS haven guest_a guest_b
        COMMAND chmod +x ${CMAKE_SOURCE_DIR}/scripts/qemu/qemu-run.sh
        COMMAND         ${CMAKE_SOURCE_DIR}/scripts/qemu/qemu-run.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Launching Haven on QEMU arm64 virt"
    )
    if(CMAKE_OBJDUMP)
        add_custom_target(disasm
            DEPENDS haven
            COMMAND ${CMAKE_OBJDUMP} -d -M no-aliases $<TARGET_FILE:haven>
            COMMENT "Disassembling haven.elf"
        )
    endif()

# ═════════════════════════════════════════════════════════════════════════════
# Host build path — unit tests, integration tests, benchmarks
# ═════════════════════════════════════════════════════════════════════════════
else()

    enable_testing()
    add_subdirectory(tests)

    # Native haven_tool (drivers/guest-tools)
    if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/guest-tools/CMakeLists.txt)
        add_subdirectory(drivers/guest-tools)
    endif()

endif() # HAVEN_ARCH_ARM64

# ═════════════════════════════════════════════════════════════════════════════
# Targets available in all build configurations
# ═════════════════════════════════════════════════════════════════════════════

# verification: Coq + Isabelle proofs (delegates to verification/Makefile)
find_program(_MAKE NAMES gmake make REQUIRED)
add_custom_target(verification
    COMMAND ${_MAKE} -C ${CMAKE_SOURCE_DIR}/verification coq
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building Coq proofs → build/verification/coq/"
)

# style-check
add_custom_target(style-check
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/dev/style-check.sh
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/compile/check-configs.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Style and configuration checks"
)

# evidence packaging
add_custom_target(evidence
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/evidence/package-evidence.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packaging thesis evidence artifacts"
)
