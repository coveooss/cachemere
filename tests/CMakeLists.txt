enable_testing()

find_package(GTest REQUIRED)
find_package(TBB REQUIRED)

add_executable(cachemere_tests)

target_link_libraries(cachemere_tests
    PRIVATE
        GTest::GTest
        GTest::Main
        tbb

        cachemere
)

target_sources(cachemere_tests
    PRIVATE
        src/cache_tests.cpp
        src/measurement_tests.cpp
        src/policy/eviction_lru_tests.cpp
        src/policy/eviction_segmented_lru_tests.cpp
        src/policy/insertion_always_tests.cpp
        src/policy/insertion_tinylfu_tests.cpp
        src/policy/detail/bloom_filter_tests.cpp
        src/policy/detail/counting_bloom_filter_tests.cpp
        src/policy/detail/hash_mixer_tests.cpp
)
