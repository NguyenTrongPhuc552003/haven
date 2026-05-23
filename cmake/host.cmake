# Haven host (native) toolchain file
# Used for building unit tests on the development machine.
# Usage: cmake -S . -B build-host -DCMAKE_TOOLCHAIN_FILE=cmake/host.cmake

# Use the host system natively — CMake picks up cc/clang/gcc from PATH.
set(CMAKE_SYSTEM_NAME    ${CMAKE_HOST_SYSTEM_NAME})
set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})

# Signal to CMakeLists.txt that this is the host/test build.
set(HAVEN_ARCH_ARM64 OFF CACHE BOOL "Build for ARM64 bare-metal" FORCE)
set(HAVEN_HOST_BUILD ON  CACHE BOOL "Build for host (unit tests)"  FORCE)
