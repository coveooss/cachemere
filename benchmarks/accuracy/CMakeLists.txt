find_package(GTest REQUIRED)
find_package(TBB REQUIRED)

add_executable(bench_accuracy)

target_link_libraries(bench_accuracy
    PRIVATE
        GTest::GTest
        GTest::Main
        tbb

        cachemere
)

target_sources(bench_accuracy
    PRIVATE
        src/io_benchmark.cpp
)
