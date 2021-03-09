#include <gtest/gtest.h>

#include <string>
#include <map>

#include "cachemere/detail/item.h"
#include "cachemere/policy/eviction_segmented_lru.h"

using namespace cachemere;

using TestSLRU = policy::EvictionSegmentedLRU<std::string, int32_t>;
using TestItem = detail::Item<std::string, int32_t>;
using ItemMap  = std::map<std::string, TestItem>;

void insert_item(std::string key, int32_t value, TestSLRU& policy, ItemMap& item_map)
{
    const auto key_and_item =
        item_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(key, key.size(), value, sizeof(int32_t))).first;

    policy.on_insert(key_and_item->second);
}

TEST(EvictionSegmentedLRU, BasicInsertEvict)
{
    TestSLRU policy;
    policy.set_protected_segment_size(4);

    ItemMap item_store;

    const std::vector<std::string> keys{"a", "b", "c", "d", "e"};

    for (int32_t i = 0; i < 5; ++i) {
        insert_item(keys[i], i, policy, item_store);
    }

    // After the loop, "a" is the coldest item in cache, and is in the probation segment because it wasn't ever loaded.
    // The first victim should be a.
    EXPECT_EQ("a", *policy.victim_begin());

    // If we touch a, it should be promoted to the protected segment.
    // The first victim should now be b.
    policy.on_cache_hit(item_store.find("a")->second);
    EXPECT_EQ("b", *policy.victim_begin());

    // Before this loop, the probation segment contains [e, d, c, b] and the protected segment contains [a].
    for (auto i = 4; i > 0; --i) {
        policy.on_cache_hit(item_store.find(keys[i])->second);
    }

    // After the last loop, the protected segment should contain [b, c, d, e] and the probation segment should contain [a].
    // This means that requesting an eviction should return a, and then e.
    EXPECT_EQ("a", *policy.victim_begin());
    EXPECT_EQ("e", *++policy.victim_begin());
}
