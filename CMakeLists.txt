cmake_minimum_required(VERSION 3.5.1)
project(kratos_runtime CXX)

set(CMAKE_CXX_STANDARD 17)

# -fpic
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# statically link glibc
set(STATIC_FLAG "-static-libgcc -static-libstdc++")

# thread is requires
find_package(Threads REQUIRED)

# extern library
add_subdirectory(extern/kratos)
add_subdirectory(extern/json11)

add_subdirectory(src)

add_subdirectory(tests/postman)