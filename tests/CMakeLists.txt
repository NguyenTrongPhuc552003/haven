# Haven host test suite — unit, integration, isolation, selftests, benchmarks
# Processed by add_subdirectory(tests) from the root CMakeLists.txt (host build only).
# Each target mirrors the exact source set from scripts/dev/test.sh.

# ── Helper: declare a single host test executable and register with CTest ─────
function(haven_test target)
    cmake_parse_arguments(_HT "" "" "SRCS;DEFINES" ${ARGN})

    add_executable(${target} ${_HT_SRCS})
    target_include_directories(${target} PRIVATE ${HAVEN_HOST_INCLUDES})
    target_compile_options(${target}     PRIVATE ${HAVEN_HOST_CFLAGS})

    if(_HT_DEFINES)
        target_compile_definitions(${target} PRIVATE ${_HT_DEFINES})
    endif()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ── Shared source aliases (absolute paths, relative to project root) ──────────
set(_S ${CMAKE_SOURCE_DIR}/src)
set(_T ${CMAKE_CURRENT_SOURCE_DIR})

set(CORE_STAGE2    ${_S}/core/mm/stage2.c)
set(CORE_OWNERSHIP ${_S}/core/irq/ownership.c)
set(CORE_BUDGET    ${_S}/core/sched/budget.c)
set(CORE_SMMU      ${_S}/core/dma/smmu.c)
set(CORE_IOMMU     ${_S}/core/iommu/iommu_policy.c)
set(CORE_TIMER     ${_S}/core/time/timer.c)
set(CORE_EL2       ${_S}/core/exc/el2_exceptions.c)

set(GUEST_UART     ${_S}/guest/drivers/uart.c)
set(GUEST_RTOS     ${_S}/guest/rtos/freertos_integration.c)

set(PRINTK_STUB    ${_T}/common/printk_stub.c)

# ── Unit tests ────────────────────────────────────────────────────────────────
haven_test(test_core_stubs
    SRCS ${_T}/unit/test_core_stubs.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
)
haven_test(test_smmu_dma
    SRCS ${_T}/unit/test_smmu_dma.c
         ${CORE_STAGE2} ${CORE_SMMU}
)
haven_test(test_el2_exceptions
    SRCS ${_T}/unit/test_el2_exceptions.c
         ${CORE_EL2}
)
haven_test(test_timer
    SRCS ${_T}/unit/test_timer.c
         ${CORE_TIMER}
)
haven_test(test_iommu_policy
    SRCS ${_T}/unit/test_iommu_policy.c
         ${CORE_IOMMU}
)
haven_test(test_guest_uart
    SRCS ${_T}/unit/test_guest_uart.c
         ${GUEST_UART}
)
haven_test(test_freertos_integration
    SRCS ${_T}/unit/test_freertos_integration.c ${PRINTK_STUB}
         ${GUEST_RTOS}
)

# ── Integration tests ─────────────────────────────────────────────────────────
haven_test(test_isolation_flow
    SRCS ${_T}/integration/test_isolation_flow.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
         ${CORE_SMMU} ${CORE_TIMER} ${CORE_IOMMU} ${CORE_EL2}
)
haven_test(test_isolation_negative
    SRCS ${_T}/integration/test_isolation_negative.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
         ${CORE_SMMU} ${CORE_TIMER} ${CORE_IOMMU} ${CORE_EL2}
)
haven_test(test_fault_injection
    SRCS ${_T}/integration/test_fault_injection.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
         ${CORE_SMMU} ${CORE_TIMER} ${CORE_IOMMU} ${CORE_EL2}
)
haven_test(test_secondary_cpu_isolation
    SRCS ${_T}/integration/test_secondary_cpu_isolation.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
)
haven_test(test_smmu_hardware
    SRCS ${_T}/integration/test_smmu_hardware.c
         ${CORE_STAGE2} ${CORE_SMMU} ${CORE_IOMMU}
)

# ── Isolation tests ───────────────────────────────────────────────────────────
haven_test(test_spatial_isolation
    SRCS ${_T}/isolation/test_spatial_isolation.c
         ${CORE_STAGE2} ${CORE_SMMU} ${CORE_IOMMU}
)
haven_test(test_temporal_isolation
    SRCS ${_T}/isolation/test_temporal_isolation.c ${PRINTK_STUB}
         ${CORE_BUDGET} ${CORE_OWNERSHIP} ${CORE_TIMER}
)

# ── Selftests ─────────────────────────────────────────────────────────────────
haven_test(test_hypervisor_invariants
    SRCS ${_T}/selftests/test_hypervisor_invariants.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
         ${CORE_IOMMU} ${CORE_SMMU} ${CORE_EL2} ${CORE_TIMER}
)

# ── Demo programs ─────────────────────────────────────────────────────────────
haven_test(demo_two_partition
    SRCS ${_T}/demos/demo_two_partition.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
         ${CORE_SMMU} ${CORE_TIMER} ${CORE_IOMMU} ${CORE_EL2}
)

# ── Benchmarks ────────────────────────────────────────────────────────────────
haven_test(bench_isolation_latency
    SRCS ${_T}/benchmarks/bench_isolation_latency.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
         ${CORE_SMMU} ${CORE_TIMER} ${CORE_IOMMU}
    DEFINES ${HAVEN_BENCH_DEFINES}
)
haven_test(bench_temporal_isolation
    SRCS ${_T}/benchmarks/bench_temporal_isolation.c ${PRINTK_STUB}
         ${CORE_BUDGET} ${CORE_TIMER}
    DEFINES ${HAVEN_BENCH_DEFINES}
)
haven_test(bench_stage2_fault
    SRCS ${_T}/benchmarks/bench_stage2_fault.c ${PRINTK_STUB}
         ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
    DEFINES ${HAVEN_BENCH_DEFINES}
)
haven_test(bench_smmu_policy
    SRCS ${_T}/benchmarks/bench_smmu_policy.c ${PRINTK_STUB}
         ${CORE_SMMU} ${CORE_STAGE2} ${CORE_OWNERSHIP} ${CORE_BUDGET}
    DEFINES ${HAVEN_BENCH_DEFINES}
)
