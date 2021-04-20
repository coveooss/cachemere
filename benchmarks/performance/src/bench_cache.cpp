#include <benchmark/benchmark.h>

#include <chrono>
#include <fstream>
#include <string>

#include "cachemere.h"

using namespace cachemere;

struct Cost {
    double operator()(const Item<std::string, std::string>& item)
    {
        return 1.0;
    }
};

template<typename Key, typename Value> using TestGDSF = policy::EvictionGDSF<Key, Value, Cost>;

#define CACHEMERE_POLICY_BENCH(test, insertion, eviction)                                                                        \
    BENCHMARK_TEMPLATE(test, insertion, eviction, true)->ArgsProduct({{1, 1000, 10000, 100000}})->Complexity()->UseManualTime(); \
    BENCHMARK_TEMPLATE(test, insertion, eviction, false)->ArgsProduct({{1, 1000, 10000, 100000}})->Complexity()->UseManualTime()

#define CACHEMERE_BENCH(test)                                                             \
    CACHEMERE_POLICY_BENCH(test, policy::InsertionAlways, policy::EvictionLRU);           \
    CACHEMERE_POLICY_BENCH(test, policy::InsertionAlways, policy::EvictionSegmentedLRU);  \
    CACHEMERE_POLICY_BENCH(test, policy::InsertionAlways, TestGDSF);                      \
    CACHEMERE_POLICY_BENCH(test, policy::InsertionTinyLFU, policy::EvictionLRU);          \
    CACHEMERE_POLICY_BENCH(test, policy::InsertionTinyLFU, policy::EvictionSegmentedLRU); \
    CACHEMERE_POLICY_BENCH(test, policy::InsertionTinyLFU, TestGDSF)

template<template<class, class> class I, template<class, class> class E, bool ThreadSafe>
using BenchCache = Cache<std::string,
                         std::string,
                         I,
                         E,
                         measurement::CapacityDynamicallyAllocated<std::string>,
                         measurement::CapacityDynamicallyAllocated<std::string>,
                         ThreadSafe>;

template<class C> std::unique_ptr<C> setup(size_t item_count)
{
    auto cache = std::make_unique<C>(item_count * 1536);

    for (size_t i = 0; i < item_count; ++i) {
        // Work extra to make sure the item is added in the cache.
        const std::string key = std::to_string(i);

        cache->find(key);
        bool ok = cache->insert(key, "some_value");

        if (!ok) {
            std::cerr << "Benchmark setup failed: not enough space to insert" << std::endl;
            exit(1);
        }
    }

    return cache;
}

template<template<class, class> class Insertion, template<class, class> class Eviction, bool ThreadSafe> void cache_insert(benchmark::State& state)
{
    const size_t previous_insertions = state.range(0);
    auto         cache               = setup<BenchCache<Insertion, Eviction, ThreadSafe>>(previous_insertions);

    for (auto _ : state) {
        const std::string key = "key";
        cache->find(key);

        const auto start = std::chrono::high_resolution_clock::now();
        cache->insert(key, "some cache value");
        const auto end = std::chrono::high_resolution_clock::now();

        const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);

        state.SetIterationTime(elapsed_seconds.count());
    }

    state.SetComplexityN(previous_insertions);
}

CACHEMERE_BENCH(cache_insert);

template<template<class, class> class Insertion, template<class, class> class Eviction, bool ThreadSafe> void cache_find(benchmark::State& state)
{
    const size_t previous_insertions = state.range(0);
    auto         cache               = setup<BenchCache<Insertion, Eviction, ThreadSafe>>(previous_insertions);

    for (auto _ : state) {
        const auto start = std::chrono::high_resolution_clock::now();
        auto       value = cache->find("0");
        benchmark::DoNotOptimize(value);
        const auto end = std::chrono::high_resolution_clock::now();

        const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
}

CACHEMERE_BENCH(cache_find);
