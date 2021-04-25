enable_testing()

find_package(GTest CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

add_executable(bench_accuracy)

target_link_libraries(bench_accuracy
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        TBB::tbb

        cachemere
)

target_sources(bench_accuracy
    PRIVATE
        src/io_benchmark.cpp
)
