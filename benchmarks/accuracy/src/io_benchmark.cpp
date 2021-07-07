#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <thread>

#include <tbb/concurrent_queue.h>

#include "cachemere.h"

static const std::string TRACE_PATH_ENV_VAR = "CACHEMERE_IO_TRACE_PATH";

std::string get_trace_path()
{
    char* trace_path = std::getenv(TRACE_PATH_ENV_VAR.c_str());
    return trace_path == nullptr ? std::string("") : std::string(trace_path);
}

struct Article {
    Article(const std::string& uri)
    {
        // Seed the RNG with a hash of our URI, this way items will have the same size & latency every time.
        std::minstd_rand rng{static_cast<std::minstd_rand::result_type>(std::hash<std::string>()(uri))};

        // Generate random sizes & latencies from two similarly-skewed gamma distributions.
        std::gamma_distribution<double> size_distribution{3, 0.8};
        std::gamma_distribution<double> latency_distribution{3, 0.5};

        m_size = static_cast<uint32_t>(size_distribution(rng) * 200 * 1024);

        // 1% of items will have a 10x higher latency.
        std::uniform_real_distribution<double> uniform_distribution{0, 1};
        const uint32_t                         multiplier = uniform_distribution(rng) >= 0.99 ? 1000 : 100;
        m_latency_ms                                      = static_cast<uint32_t>(latency_distribution(rng) * multiplier);
    }

    [[nodiscard]] size_t size() const
    {
        return m_size;
    }

    size_t m_size;
    size_t m_latency_ms;
};

template<typename Cache> void run_benchmark(Cache cache)
{
    const uint32_t NB_THREADS =
        std::max(std::thread::hardware_concurrency(), static_cast<unsigned int>(2));  // We need at least two threads (main thread + one worker).
    const uint32_t NB_WORKERS = NB_THREADS - 1;

    std::atomic<bool>     is_running{true};
    std::atomic<uint32_t> operation_count{0};
    std::atomic<uint32_t> miss_count{0};
    std::atomic<uint32_t> total_latency{0};

    std::vector<std::thread> workers;

    tbb::concurrent_queue<std::string> work_queue;

    for (size_t i = 0; i < NB_WORKERS; ++i) {
        workers.emplace_back([&]() {
            std::string work_item;
            while (is_running) {
                while (work_queue.try_pop(work_item)) {
                    std::optional<std::shared_ptr<Article>> fetched_article = cache->find(work_item);
                    ++operation_count;

                    if (fetched_article) {
                        // Cache Hit.
                    } else {
                        // Cache Miss.
                        auto article = std::make_shared<Article>(work_item);
                        cache->insert(work_item, article);
                        total_latency += static_cast<uint32_t>(article->m_latency_ms);
                        ++miss_count;
                    }
                }
            }
        });
    }

    std::ifstream infile{get_trace_path()};

    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream       iss{line};
        std::vector<std::string> splitted{std::istream_iterator<std::string>(iss), {}};

        const auto article_uri = splitted.at(2);
        work_queue.push(article_uri);
    }
    is_running = false;

    for (auto& thread : workers) {
        thread.join();
    }

    const auto hit_rate         = cache->hit_rate();
    const auto byte_hit_rate    = cache->byte_hit_rate();
    const auto avg_latency_ms   = static_cast<double>(total_latency) / static_cast<double>(operation_count);
    const auto avg_miss_latency = static_cast<double>(total_latency) / static_cast<double>(miss_count);

    std::cout << "Hit Rate: " << hit_rate * 100 << "%" << std::endl;
    std::cout << "Byte Hit Rate: " << byte_hit_rate / 1000 << "kb/request" << std::endl;
    std::cout << "Avg. Latency: " << avg_latency_ms << "ms" << std::endl;
    std::cout << "Avg. Miss Latency: " << avg_miss_latency << "ms" << std::endl;
}

class CacheSizeFixture : public testing::TestWithParam<uint32_t>
{
public:
    static std::vector<uint32_t> get_tested_sizes()
    {
        std::vector<uint32_t> sizes;

        // Test caches from 1mb to 512mb
        for (size_t i = 1; i <= 512; i += 4) {
            sizes.push_back(1024 * 1024 * i);
        }

        return sizes;
    }
};

INSTANTIATE_TEST_SUITE_P(AccuracyBenchmark, CacheSizeFixture, testing::ValuesIn(CacheSizeFixture::get_tested_sizes()));

TEST_P(CacheSizeFixture, IO_LRU)
{
    using LRUCache = cachemere::presets::memory::LRUCache<std::string,
                                                          std::shared_ptr<Article>,
                                                          cachemere::measurement::Size<Article>,
                                                          cachemere::measurement::CapacityDynamicallyAllocated<std::string>>;

    const uint32_t cache_size = GetParam();
    auto           cache      = std::make_shared<LRUCache>(cache_size);
    run_benchmark(cache);
}

TEST_P(CacheSizeFixture, IO_TINYLFU)
{
    using TLFUCache = cachemere::presets::memory::TinyLFUCache<std::string,
                                                               std::shared_ptr<Article>,
                                                               cachemere::measurement::Size<Article>,
                                                               cachemere::measurement::CapacityDynamicallyAllocated<std::string>>;

    const uint32_t cache_size = GetParam();
    auto           cache      = std::make_shared<TLFUCache>(cache_size);

    run_benchmark(cache);
}

TEST_P(CacheSizeFixture, IO_GDSF_CONSTANT_COST)
{
    struct ConstantCost {
        double operator()(const std::string& /* key */, const cachemere::Item<std::shared_ptr<Article>>& /* item */)
        {
            return 1;
        }
    };

    using GDSFCache = cachemere::presets::memory::CustomCostCache<std::string,
                                                                  std::shared_ptr<Article>,
                                                                  ConstantCost,
                                                                  cachemere::measurement::Size<Article>,
                                                                  cachemere::measurement::CapacityDynamicallyAllocated<std::string>>;

    const uint32_t cache_size = GetParam();

    auto cache = std::make_shared<GDSFCache>(cache_size);

    run_benchmark(cache);
}

TEST_P(CacheSizeFixture, IO_GDSF_LATENCY_COST)
{
    struct QuadraticCost {
        double operator()(const std::string& /* key */, const cachemere::Item<std::shared_ptr<Article>>& item)
        {
            return static_cast<double>(item.m_value->m_latency_ms);
        }
    };

    using GDSFCache = cachemere::presets::memory::CustomCostCache<std::string,
                                                                  std::shared_ptr<Article>,
                                                                  QuadraticCost,
                                                                  cachemere::measurement::Size<Article>,
                                                                  cachemere::measurement::CapacityDynamicallyAllocated<std::string>>;

    const uint32_t cache_size = GetParam();

    auto cache = std::make_shared<GDSFCache>(cache_size);

    run_benchmark(cache);
}
