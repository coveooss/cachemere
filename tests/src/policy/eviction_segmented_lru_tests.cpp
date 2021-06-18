#include <gtest/gtest.h>

#include <string>
#include <map>

#include "cachemere/item.h"
#include "cachemere/policy/eviction_segmented_lru.h"

using namespace cachemere;

using TestSLRU = policy::EvictionSegmentedLRU<std::string, int32_t>;
using TestItem = Item<int32_t>;
using ItemMap  = std::map<std::string, TestItem>;

void insert_item(std::string key, int32_t value, TestSLRU& policy, ItemMap& item_map)
{
    const auto key_and_item =
        item_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(key.size(), value, sizeof(int32_t))).first;

    policy.on_insert(key_and_item->first, key_and_item->second);
}

void expect_victims(const TestSLRU& policy, std::vector<std::string> expected_victims)
{
    std::vector<std::string> victims;
    for (auto it = policy.victim_begin(); it != policy.victim_end(); ++it) {
        victims.push_back(*it);
    }
    EXPECT_EQ(victims, expected_victims);
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
    auto key_and_item = item_store.find("a");
    policy.on_cache_hit(key_and_item->first, key_and_item->second);
    EXPECT_EQ("b", *policy.victim_begin());

    // Before this loop, the probation segment contains [e, d, c, b] and the protected segment contains [a].
    for (auto i = 4; i > 0; --i) {
        auto key_and_item = item_store.find(keys[i]);
        policy.on_cache_hit(key_and_item->first, key_and_item->second);
    }

    // After the last loop, the protected segment should contain [b, c, d, e] and the probation segment should contain [a].
    // This means that requesting an eviction should return a, and then e.
    EXPECT_EQ("a", *policy.victim_begin());
    EXPECT_EQ("e", *++policy.victim_begin());
}

TEST(EvictionSegmentedLRU, RandomEvictions)
{
    TestSLRU policy;
    policy.set_protected_segment_size(4);

    ItemMap item_store;

    const std::vector<std::string> keys{"a", "b", "c", "d", "e"};

    for (int32_t i = 0; i < 5; ++i) {
        insert_item(keys[i], i, policy, item_store);
    }

    // Promote b, c, and d to the protected segment.
    for (const auto& key : {"b", "c", "d"}) {
        auto key_and_item = item_store.find(key);
        policy.on_cache_hit(key_and_item->first, key_and_item->second);
    }

    // Verify everything is in order.
    expect_victims(policy, {"a", "e", "b", "c", "d"});

    // Remove something not at the head of the probation segment.
    policy.on_evict("e");
    expect_victims(policy, {"a", "b", "c", "d"});

    // Remove something in the protected segment
    policy.on_evict("c");
    expect_victims(policy, {"a", "b", "d"});
}
