find_package(GTest REQUIRED)

add_executable(cachemere_tests)

target_link_libraries(cachemere_tests
        PRIVATE
        GTest::GTest
        GTest::Main

        absl::container_common
        absl::flat_hash_map
        absl::node_hash_map

        cachemere
        )

target_sources(cachemere_tests
        PRIVATE
        src/count_cache_tests.cpp
        src/memory_cache_tests.cpp
        src/cache_tests.cpp
        src/measurement_tests.cpp
        src/policy/constraint_count_tests.cpp
        src/policy/constraint_memory_tests.cpp
        src/policy/eviction_lru_tests.cpp
        src/policy/eviction_gdsf_tests.cpp
        src/policy/eviction_segmented_lru_tests.cpp
        src/policy/insertion_always_tests.cpp
        src/policy/insertion_tinylfu_tests.cpp
        src/policy/detail/bloom_filter_tests.cpp
        src/policy/detail/counting_bloom_filter_tests.cpp
        src/policy/detail/hash_mixer_tests.cpp
        )
