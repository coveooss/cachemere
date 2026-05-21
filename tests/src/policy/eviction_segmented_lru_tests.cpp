#include <gtest/gtest.h>

#include <ranges>
#include <string>
#include <map>

#include <absl/hash/hash.h>

#include "cachemere/item.h"
#include "cachemere/policy/eviction_segmented_lru.h"

using namespace cachemere;

using TestSLRU = policy::EvictionSegmentedLRU<std::string, absl::Hash<std::string>, int32_t>;
using TestItem = Item<int32_t>;
using ItemMap  = std::map<std::string, TestItem>;

void insert_item(std::string key, int32_t value, TestSLRU& policy, ItemMap& item_map)
{
    const auto key_and_item =
        item_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(key.size(), value, sizeof(int32_t))).first;

    policy.on_insert(key_and_item->first, key_and_item->second);
}

void expect_victims(TestSLRU& policy, const std::vector<std::string>& expected_victims)
{
    std::vector<TestSLRU::Ticket> popped_victims;
    std::vector<std::string>      victims;

    for (size_t i = 0; i < expected_victims.size(); ++i) {
        auto victim = policy.pop_victim();
        victims.push_back(victim.key());
        popped_victims.push_back(victim);
    }

    for (auto& popped_victim : std::ranges::reverse_view(popped_victims)) {
        policy.rollback(popped_victim);
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
    auto victim = policy.pop_victim();
    EXPECT_EQ("a", victim.key());
    policy.rollback(victim);

    // If we touch a, it should be promoted to the protected segment.
    // The first victim should now be b.
    auto key_and_item = item_store.find("a");
    policy.on_cache_hit(key_and_item->first, key_and_item->second);
    victim = policy.pop_victim();
    EXPECT_EQ("b", victim.key());
    policy.rollback(victim);

    // Before this loop, the probation segment contains [e, d, c, b] and the protected segment contains [a].
    for (auto i = 4; i > 0; --i) {
        auto key_and_item = item_store.find(keys[i]);
        policy.on_cache_hit(key_and_item->first, key_and_item->second);
    }

    // After the last loop, the protected segment should contain [b, c, d, e] and the probation segment should contain [a].
    // This means that requesting an eviction should return a, and then e.
    expect_victims(policy, {"a", "e", "d", "c", "b"});
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
    policy.on_evict("e", Item{0, 4, sizeof(uint32_t)});
    expect_victims(policy, {"a", "b", "c", "d"});

    // Remove something in the protected segment
    policy.on_evict("c", Item{0, 2, sizeof(uint32_t)});
    expect_victims(policy, {"a", "b", "d"});
}
