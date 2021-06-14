#include <gtest/gtest.h>

#include <list>
#include <atomic>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>

#include "cachemere/cache.h"
#include "cachemere/measurement.h"
#include "cachemere/presets.h"

struct Point3D {
    Point3D(uint32_t a, uint32_t b, uint32_t c) : x(a), y(b), z(c)
    {
    }

    uint32_t x;
    uint32_t y;
    uint32_t z;
};

using namespace cachemere;

using LRUCache     = presets::LRUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;
using TinyLFUCache = presets::TinyLFUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;

struct RandomCost {
    double operator()(const Item<uint32_t, Point3D>& /* item */)
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

    std::shared_ptr<CacheT> new_cache(std::vector<std::pair<uint32_t, Point3D>> collection, size_t size)
    {
        return std::make_shared<CacheT>(collection, size);
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

TYPED_TEST(CacheTest, RemoveWhenKeyPresent)
{
    auto cache = TestFixture::new_cache(10 * sizeof(Point3D));

    cache->find(0);
    cache->insert(0, Point3D{0, 0, 0});

    EXPECT_TRUE(cache->contains(0));
    EXPECT_TRUE(cache->remove(0));
    EXPECT_FALSE(cache->contains(0));
}

TYPED_TEST(CacheTest, RemoveWhenKeyAbsent)
{
    auto cache = TestFixture::new_cache(10 * sizeof(Point3D));
    EXPECT_FALSE(cache->remove(0));
}

TYPED_TEST(CacheTest, Retain)
{
    auto cache = TestFixture::new_cache(10 * sizeof(Point3D));

    const uint32_t number_of_items = 5;
    for (uint32_t point_id = 0; point_id < number_of_items; ++point_id) {
        cache->find(point_id);
        cache->insert(point_id, Point3D{point_id, point_id, point_id});
    }

    cache->retain([](const uint32_t& key, const Point3D& /* value */) { return key % 2 == 0; });

    for (uint32_t point_id = 0; point_id < number_of_items; ++point_id) {
        if (point_id % 2 == 0) {
            EXPECT_TRUE(cache->contains(point_id));
        } else {
            EXPECT_FALSE(cache->contains(point_id));
        }
    }
}

TYPED_TEST(CacheTest, Collect)
{
    const uint32_t                            number_of_items = 5;
    std::vector<std::pair<uint32_t, Point3D>> cache_items;
    for (uint32_t point_id = 0; point_id < number_of_items; ++point_id) {
        cache_items.emplace_back(point_id, Point3D{point_id, point_id, point_id});
    }

    auto cache = TestFixture::new_cache(cache_items, 10 * sizeof(Point3D));

    // Test with various STL collection types.
    std::vector<std::pair<uint32_t, Point3D>> item_vec;
    cache->collect_into(item_vec);
    EXPECT_EQ(item_vec.size(), number_of_items);

    std::map<uint32_t, Point3D> item_map;
    cache->collect_into(item_map);
    EXPECT_EQ(item_map.size(), number_of_items);

    std::list<std::pair<uint32_t, Point3D>> item_list;
    cache->collect_into(item_list);
    EXPECT_EQ(item_list.size(), number_of_items);

    std::unordered_map<uint32_t, Point3D> item_unordered_map;
    cache->collect_into(item_unordered_map);
    EXPECT_EQ(item_unordered_map.size(), number_of_items);

    // Test with a custom collection that doesn't support size or reserve.
    struct CustomCollection {
        void emplace(const uint32_t& key, const Point3D& value)
        {
            m_data.emplace(key, value);
        }

        std::map<uint32_t, Point3D> m_data;
    };

    CustomCollection items_custom;
    cache->collect_into(items_custom);
    EXPECT_EQ(items_custom.m_data.size(), number_of_items);
}

TYPED_TEST(CacheTest, Swap)
{
    auto cache_even = TestFixture::new_cache(10 * sizeof(Point3D));
    auto cache_odd  = TestFixture::new_cache(10 * sizeof(Point3D));

    const uint32_t number_of_items = 10;

    for (uint32_t point_id = 0; point_id < number_of_items; ++point_id) {
        auto& target_cache = point_id % 2 == 0 ? cache_even : cache_odd;

        target_cache->find(point_id);
        target_cache->insert(point_id, Point3D{point_id, point_id, point_id});
    }

    using std::swap;
    swap(*cache_even, *cache_odd);

    EXPECT_TRUE(cache_even->contains(7));
    EXPECT_TRUE(cache_odd->contains(4));

    EXPECT_TRUE(
        cache_odd->find(4)
            .has_value());  // Cache::find() hits the policy on cache hit. If this call doesn't throw an assert it means the policies were swapped properly.

    EXPECT_FALSE(cache_even->contains(2));
}

TYPED_TEST(CacheTest, Clear)
{
    auto cache = TestFixture::new_cache({{1, Point3D{1, 1, 1}}, {2, Point3D{2, 2, 2}}}, 10 * sizeof(Point3D));

    EXPECT_TRUE(cache->contains(1));
    EXPECT_TRUE(cache->contains(2));

    cache->clear();

    EXPECT_FALSE(cache->contains(1));
    EXPECT_FALSE(cache->contains(2));
}

TYPED_TEST(CacheTest, ImportConstructionNotEnoughSpace)
{
    auto cache = TestFixture::new_cache({{1, Point3D{1, 1, 1}}, {2, Point3D{2, 2, 2}}, {3, Point3D{3, 3, 3}}},
                                        4 * sizeof(Point3D));  // Only 2 items fit because of overhead.

    EXPECT_TRUE(cache->contains(1));
    EXPECT_TRUE(cache->contains(2));
    EXPECT_FALSE(cache->contains(3));
}

TEST(CacheTest, NoValueCopyOnInsert)
{
    using PtrCache =
        presets::LRUCache<std::string, std::unique_ptr<Point3D>, measurement::SizeOf<Point3D>, measurement::CapacityDynamicallyAllocated<std::string>>;

    PtrCache cache{10 * sizeof(Point3D)};

    auto item = std::make_unique<Point3D>(1, 1, 1);
    EXPECT_TRUE(cache.insert("asdf", std::move(item)));
}

TEST(CacheTest, NoValueCopyOnImportConstruction)
{
    using PtrCache =
        presets::LRUCache<std::string, std::unique_ptr<Point3D>, measurement::SizeOf<Point3D>, measurement::CapacityDynamicallyAllocated<std::string>>;

    std::vector<std::pair<std::string, std::unique_ptr<Point3D>>> items;
    items.emplace_back("a", std::make_unique<Point3D>(1, 1, 1));
    items.emplace_back("b", std::make_unique<Point3D>(2, 2, 2));

    PtrCache cache{items, 10 * sizeof(Point3D)};

    EXPECT_TRUE(cache.contains("a"));
}
