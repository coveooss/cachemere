/// Tests that are specific to the memory-constrained cache.
#include <gtest/gtest.h>

#include "cachemere/cache.h"
#include "cachemere/measurement.h"
#include "cachemere/presets.h"

using namespace cachemere;

struct Point3D {
    Point3D(uint32_t a, uint32_t b, uint32_t c) : x(a), y(b), z(c)
    {
    }

    uint32_t x;
    uint32_t y;
    uint32_t z;
};

using MemoryLRUCache     = presets::memory::LRUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;
using MemoryTinyLFUCache = presets::memory::TinyLFUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;

struct RandomCost {
    double operator()(const uint32_t& /* key */, const Item<Point3D>& /* item */)
    {
        const int min = 0;
        const int max = 100;

        return static_cast<double>(min + (rand() % static_cast<int>(max - min + 1)));
    }
};

using MemoryCustomCostCache = presets::memory::CustomCostCache<uint32_t, Point3D, RandomCost, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;

using TestTypes = testing::Types<MemoryLRUCache, MemoryTinyLFUCache, MemoryCustomCostCache>;

template<typename CacheT> class MemoryCacheTest : public testing::Test
{
public:
    std::shared_ptr<CacheT> new_cache(size_t size)
    {
        return std::make_shared<CacheT>(size);
    }

    std::shared_ptr<CacheT> new_cache(std::vector<std::pair<uint32_t, Point3D>> collection, size_t size)
    {
        return std::make_shared<CacheT>(collection, std::make_tuple(size));
    }
};

TYPED_TEST_SUITE(MemoryCacheTest, TestTypes);

TYPED_TEST(MemoryCacheTest, Resize)
{
    const size_t original_size = 10 * (sizeof(Point3D) + sizeof(uint32_t));
    auto         cache         = TestFixture::new_cache(original_size);

    // Insert all items and make sure they all fit in cache.
    const uint32_t number_of_items = 5;
    for (uint32_t point_id = 0; point_id < number_of_items; ++point_id) {
        cache->find(point_id);  // Trigger a cache miss so TinyLFU has seen the item once.
        cache->insert(point_id, Point3D{point_id, point_id, point_id});
    }
    EXPECT_EQ(cache->number_of_items(), number_of_items);

    // Resize the cache, make sure some were evicted.
    size_t new_cache_size = 2 * (sizeof(Point3D) + sizeof(uint32_t));
    cache->update_constraint(new_cache_size);
    EXPECT_LE(cache->constraint_policy().memory(), new_cache_size);
    EXPECT_EQ(cache->number_of_items(), 2);
}

TYPED_TEST(MemoryCacheTest, ImportConstructionNotEnoughSpace)
{
    auto cache = TestFixture::new_cache({{1, Point3D{1, 1, 1}}, {2, Point3D{2, 2, 2}}, {3, Point3D{3, 3, 3}}}, 2 * (sizeof(uint32_t) + sizeof(Point3D)));

    EXPECT_TRUE(cache->contains(1));
    EXPECT_TRUE(cache->contains(2));
    EXPECT_FALSE(cache->contains(3));
}
