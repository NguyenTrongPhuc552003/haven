# Haven guest userspace tool — native host build
# Replaces drivers/guest-tools/Makefile
# Built only when NOT cross-compiling (HAVEN_HOST_BUILD or direct host cmake run).

add_executable(haven_tool haven_tool.c)

target_include_directories(haven_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(haven_tool PRIVATE -std=c11 -Wall -Wextra)
