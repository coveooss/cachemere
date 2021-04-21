cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(USE_VCPKG "Use vcpkg for dependencies" OFF)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if (USE_VCPKG)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
    CACHE STRING "Vcpkg toolchain file")

    if (BUILD_TESTS)
        list(APPEND VCPKG_MANIFEST_FEATURES "tests")
    endif()

    if (BUILD_BENCHMARKS)
        list(APPEND VCPKG_MANIFEST_FEATURES "benchmarks")
    endif()
endif()


project(cachemere)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.65 CONFIG REQUIRED)

add_library(cachemere INTERFACE)

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  # using Clang or GCC
  add_compile_options("-Wall" "-Wextra")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # using MSVC
  add_compile_options("/W4")
endif()

target_link_libraries(cachemere
    INTERFACE
        Boost::boost
)

target_include_directories(cachemere
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

target_sources(cachemere
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/cache.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/cache.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/item.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/measurement.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/measurement.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/presets.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/detail/traits.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/eviction_lru.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/eviction_lru.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/eviction_gdsf.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/eviction_gdsf.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/eviction_segmented_lru.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/eviction_segmented_lru.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/insertion_always.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/insertion_always.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/insertion_tinylfu.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/insertion_tinylfu.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/bloom_filter.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/bloom_filter.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/bloom_filter_math.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/bloom_filter_math.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/counting_bloom_filter.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/counting_bloom_filter.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/hash_mixer.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/cachemere/policy/detail/hash_mixer.hpp>
)

if (BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if (BUILD_TESTS)
    add_subdirectory(tests)
endif()
