find_package(benchmark REQUIRED)

add_executable(bench_performance)

target_link_libraries(bench_performance
    PRIVATE
        benchmark::benchmark

        cachemere
)


target_sources(bench_performance
    PRIVATE
    src/bench_cache.cpp
    src/main.cpp
)
