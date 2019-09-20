cmake_minimum_required(VERSION 3.5.1)
project(kratos_runtime CXX)

set(CMAKE_CXX_STANDARD 11)

# -fpic
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# statically link glibc
set(CMAKE_EXE_LINKER_FLAGS " -static")
set(STATIC_FLAG "-static-libgcc -static-libstdc++")

# thread is requires
find_package(Threads REQUIRED)

# extern library
add_subdirectory(extern/fmt)
add_subdirectory(extern/seasocks/)

add_subdirectory(src)