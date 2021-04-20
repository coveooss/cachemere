project(cachemere)

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.65 REQUIRED)

add_library(cachemere INTERFACE)
add_compile_options("-Wall")

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

add_subdirectory(benchmarks)
add_subdirectory(tests)
