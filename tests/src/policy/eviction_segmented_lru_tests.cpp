#include <gtest/gtest.h>

#include <map>

#include "cachemere/detail/item.h"
#include "cachemere/policy/eviction_segmented_lru.h"

using namespace cachemere;

using TestSLRU = policy::EvictionSegmentedLRU<int32_t, int32_t>;
using TestItem = detail::Item<int32_t, int32_t>;
using ItemMap  = std::map<int32_t, TestItem>;

void insert_item(int32_t key, TestSLRU& policy, ItemMap& item_map)
{
    const auto key_and_item =
        item_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(key, sizeof(int32_t), key, sizeof(int32_t))).first;

    policy.on_insert(key_and_item->second);
}

TEST(EvictionSegmentedLRU, BasicInsertEvict)
{
    TestSLRU policy;
    policy.set_protected_segment_size(4);

    ItemMap item_store;

    for (int32_t i = 0; i < 5; ++i) {
        insert_item(i, policy, item_store);
    }

    // After the loop, "0" is the coldest item in cache, and is in the probation segment because it wasn't ever loaded.
    // The first victim should be 0.
    EXPECT_EQ(0, *policy.victim_begin());

    // If we touch 0, it should be promoted to the protected segment.
    // The first victim should now be 1.
    policy.on_cache_hit(item_store.find(0)->second);
    EXPECT_EQ(1, *policy.victim_begin());

    // Before this loop, the probation segment contains [4, 3, 2, 1] and the protected segment contains [0].
    for (auto i = 4; i > 0; --i) {
        policy.on_cache_hit(item_store.find(i)->second);
    }

    // After the last loop, the protected segment should contain [1, 2, 3, 4] and the probation segment should contain [0].
    // This means that requesting an eviction should return 0.
    EXPECT_EQ(0, *policy.victim_begin());
    EXPECT_EQ(4, *++policy.victim_begin());
}
