cmake_minimum_required(VERSION 3.5.1)
project(kratos_runtime CXX)

set(CMAKE_CXX_STANDARD 17)

# -fpic
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# statically link glibc
set(STATIC_FLAG "-static-libgcc -static-libstdc++")

# default to release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Choose the type of build, options are: Debug Release"
            FORCE)
endif (NOT CMAKE_BUILD_TYPE)

# thread is requires
find_package(Threads REQUIRED)

# extern library
add_subdirectory(extern/kratos)
add_subdirectory(extern/json11)

add_subdirectory(src)

include(GoogleTest)
enable_testing()
add_subdirectory(tests)
