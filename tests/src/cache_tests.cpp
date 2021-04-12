#include <gtest/gtest.h>

#include <random>
#include <string>
#include <thread>

#include "cachemere/cache.h"
#include "cachemere/measurement.h"
#include "cachemere/presets.h"

struct Point3D {
    uint32_t x;
    uint32_t y;
    uint32_t z;
};

using namespace cachemere;

using LRUCache     = presets::LRUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;
using TinyLFUCache = presets::TinyLFUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;

struct RandomCost {
    double operator()(const Item<uint32_t, Point3D>& item)
    {
        const int min = 0;
        const int max = 100;

        return static_cast<double>(min + (rand() % static_cast<int>(max - min + 1)));
    }
};

using CustomCostCache = presets::CustomCostCache<uint32_t, Point3D, RandomCost, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;

template<typename CacheT> class CacheTest : public testing::Test
{
public:
    std::shared_ptr<CacheT> new_cache(size_t size)
    {
        return std::make_shared<CacheT>(size);
    }
};

using TestTypes = testing::Types<LRUCache, TinyLFUCache, CustomCostCache>;

TYPED_TEST_SUITE(CacheTest, TestTypes);

TYPED_TEST(CacheTest, SingleThread)
{
    auto cache = TestFixture::new_cache(150);
    for (auto i = 0; i < 50; ++i) {
        for (uint32_t point_id = 0; point_id < 4; ++point_id) {
            auto fetched = cache->find(point_id);

            if (fetched.has_value()) {
                EXPECT_EQ(point_id, (*fetched).x);
            } else {
                Point3D point{point_id, point_id, point_id};
                cache->insert(point_id, point);
            }
        }
    }

    const double hit_rate = cache->hit_rate();
    EXPECT_GT(hit_rate, 0.8);
}

TYPED_TEST(CacheTest, MultiThreadLong)
{
    const size_t item_count          = 10000;
    const size_t nb_inserter_threads = 5;

    // Prepare the test data.
    std::vector<Point3D> points;
    points.reserve(item_count);

    for (uint32_t i = 0; i < item_count; ++i) {
        points.push_back(Point3D{i, i, i});
    }

    std::vector<std::thread> workers;
    std::atomic<bool>        is_running = true;
    std::atomic<uint32_t>    op_count   = 0;
    std::atomic<uint32_t>    errors     = 0;
    auto                     cache      = TestFixture::new_cache(3000);

    std::cout << "Starting workers" << std::endl;
    for (size_t i = 0; i < nb_inserter_threads; ++i) {
        workers.emplace_back([&]() {
            std::default_random_engine            rng;
            std::uniform_int_distribution<size_t> distribution{0, item_count - 1};

            while (is_running) {
                const size_t idx = distribution(rng);
                assert(idx < points.size());
                const auto point = points[idx];

                const auto fetchedPoint = cache->find(point.x);
                if (fetchedPoint) {
                    // Cache hit.
                    // Multi-threaded gtest assertions are unsafe on windows,
                    // so we collect the errors and assert on the main thread.
                    if (point.x != fetchedPoint->x) {
                        ++errors;
                    }
                } else {
                    // Cache miss.
                    cache->insert(point.x, point);
                }
                ++op_count;
            }
        });
    }

    std::cout << "Waiting..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    is_running = false;
    for (auto& thread : workers) {
        thread.join();
    }

    std::cout << "Done." << std::endl;

    EXPECT_EQ(0, errors);

    std::cout << "Total of " << op_count << " operations in 10.0s";
    std::cout << "Hit rate: " << cache->hit_rate() << std::endl;
}

TYPED_TEST(CacheTest, Resize)
{
    auto cache = TestFixture::new_cache(10 * sizeof(Point3D));

    // Insert all items and make sure they all fit in cache.
    const uint32_t number_of_items = 5;
    for (uint32_t point_id = 0; point_id < number_of_items; ++point_id) {
        cache->find(point_id);  // Trigger a cache miss so TinyLFU has seen the item once.
        cache->insert(point_id, Point3D{point_id, point_id, point_id});
    }
    EXPECT_EQ(cache->number_of_items(), number_of_items);

    // Resize the cache, make sure some were evicted.
    cache->set_maximum_size(4 * sizeof(Point3D));
    EXPECT_LE(cache->size(), 4 * sizeof(Point3D));
    EXPECT_EQ(cache->number_of_items(), 2);  // Only two items fit because of the cache overhead.
}
