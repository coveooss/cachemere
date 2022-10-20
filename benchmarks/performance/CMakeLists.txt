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

add_compile_definitions(BENCHMARK_STATIC_DEFINE) # See https://stackoverflow.com/questions/73494386/lnk2001-linker-error-while-linking-google-benchmark-lib