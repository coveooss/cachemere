enable_testing()

find_package(GTest REQUIRED)

add_executable(cachemere_tests)

target_link_libraries(cachemere_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main

        cachemere
)

target_sources(cachemere_tests
    PRIVATE
        src/cache_tests.cpp
        src/measurement_tests.cpp
        src/policy/eviction_lru_tests.cpp
        src/policy/eviction_gdsf_tests.cpp
        src/policy/eviction_segmented_lru_tests.cpp
        src/policy/insertion_always_tests.cpp
        src/policy/insertion_tinylfu_tests.cpp
        src/policy/detail/bloom_filter_tests.cpp
        src/policy/detail/counting_bloom_filter_tests.cpp
        src/policy/detail/hash_mixer_tests.cpp
)
