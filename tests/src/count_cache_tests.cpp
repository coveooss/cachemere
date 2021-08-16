/// Tests that are specific to the count-constrained cache.
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

struct RandomCost {
    double operator()(const uint32_t& /* key */, const Item<Point3D>& /* item */)
    {
        const int min = 0;
        const int max = 100;

        return static_cast<double>(min + (rand() % static_cast<int>(max - min + 1)));
    }
};

using CountLRUCache        = presets::count::LRUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;
using CountTinyLFUCache    = presets::count::TinyLFUCache<uint32_t, Point3D, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;
using CountCustomCostCache = presets::count::CustomCostCache<uint32_t, Point3D, RandomCost, measurement::SizeOf<Point3D>, measurement::SizeOf<uint32_t>>;

using TestTypes = testing::Types<CountLRUCache, CountTinyLFUCache, CountCustomCostCache>;

template<typename CacheT> class CountCacheTest : public testing::Test
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

TYPED_TEST_SUITE(CountCacheTest, TestTypes);

TYPED_TEST(CountCacheTest, Resize)
{
    const size_t original_item_count = 10;

    auto cache = TestFixture::new_cache(original_item_count);

    for (uint32_t point_id = 0; point_id < original_item_count; ++point_id) {
        cache->find(point_id);  // Trigger a cache miss so TinyLFU has seen the item once.
        cache->insert(point_id, Point3D{point_id, point_id, point_id});
    }

    EXPECT_EQ(cache->number_of_items(), original_item_count);
    cache->update_constraint(3);
    EXPECT_EQ(cache->number_of_items(), 3);
}
