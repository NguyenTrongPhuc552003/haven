# SPDX-License-Identifier: Apache-2.0
# Reproducible builds (docs/methodology/stage2/20261109-d5-platform-layout,
# Step 1). src/core/init.c prints __DATE__ and __TIME__, which GCC takes
# from SOURCE_DATE_EPOCH (in UTC) when it is set. Unless the environment
# sets it at configure time, every C compile gets the HEAD commit's time,
# so two builds of one commit give byte-identical images.
# Include before any target is defined.

set(HAVEN_SOURCE_DATE_EPOCH "")
if(DEFINED ENV{SOURCE_DATE_EPOCH})
    set(HAVEN_SOURCE_DATE_EPOCH "$ENV{SOURCE_DATE_EPOCH}")
    set(_haven_sde_from "environment")
    if(NOT HAVEN_SOURCE_DATE_EPOCH MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "SOURCE_DATE_EPOCH='${HAVEN_SOURCE_DATE_EPOCH}' is not a "
            "non-negative integer")
    endif()
else()
    find_program(HAVEN_GIT_EXECUTABLE git)
    if(HAVEN_GIT_EXECUTABLE)
        execute_process(
            COMMAND ${HAVEN_GIT_EXECUTABLE} log -1 --format=%ct
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _haven_sde
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _haven_sde_rc
            ERROR_QUIET)
        execute_process(
            COMMAND ${HAVEN_GIT_EXECUTABLE} rev-parse --absolute-git-dir
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _haven_git_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_haven_sde_rc EQUAL 0 AND _haven_sde MATCHES "^[0-9]+$")
            set(HAVEN_SOURCE_DATE_EPOCH "${_haven_sde}")
            set(_haven_sde_from "HEAD commit time")
        endif()
    endif()
endif()

if(HAVEN_SOURCE_DATE_EPOCH STREQUAL "")
    message(STATUS "Haven SOURCE_DATE_EPOCH: not set (no git checkout); "
                   "the build time is embedded")
    return()
endif()

# A commit or checkout moves HEAD: configure again for the new time.
if(_haven_sde_from STREQUAL "HEAD commit time" AND _haven_git_dir)
    if(EXISTS "${_haven_git_dir}/logs/HEAD")
        set_property(DIRECTORY APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS "${_haven_git_dir}/logs/HEAD")
    elseif(EXISTS "${_haven_git_dir}/HEAD")
        set_property(DIRECTORY APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS "${_haven_git_dir}/HEAD")
    endif()
endif()

# cmake -E env is CMake's own portable tool; a launcher the user set
# (for example ccache) still runs, after it.
set(CMAKE_C_COMPILER_LAUNCHER
    ${CMAKE_COMMAND} -E env SOURCE_DATE_EPOCH=${HAVEN_SOURCE_DATE_EPOCH}
    ${CMAKE_C_COMPILER_LAUNCHER})
message(STATUS "Haven SOURCE_DATE_EPOCH: ${HAVEN_SOURCE_DATE_EPOCH} "
               "(${_haven_sde_from})")
