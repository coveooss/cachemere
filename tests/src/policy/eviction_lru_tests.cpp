#include <gtest/gtest.h>

#include <map>
#include <tuple>
#include <vector>

#include "cachemere/detail/item.h"
#include "cachemere/policy/eviction_lru.h"

using namespace cachemere;

using TestLRU  = policy::EvictionLRU<int32_t, int32_t>;
using TestItem = detail::Item<int32_t, int32_t>;
using ItemMap  = std::map<int32_t, TestItem>;

void insert_item(int32_t key, int32_t value, TestLRU& policy, ItemMap& item_map)
{
    const auto key_and_item =
        item_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(key, sizeof(int32_t), value, sizeof(int32_t))).first;

    policy.on_insert(key_and_item->second);
}

TEST(EvictionLRU, EvictionsWithoutReordering)
{
    TestLRU policy;

    // Policies store references - we need to keep the values alive ourselves.
    ItemMap item_store;

    insert_item(42, 42, policy, item_store);
    insert_item(18, 18, policy, item_store);
    insert_item(1337, 1337, policy, item_store);

    const auto victim = *policy.victim_begin();
    policy.on_evict(victim);

    EXPECT_EQ(42, victim);
}

TEST(EvictionLRU, NoOpReordering)
{
    TestLRU policy;

    ItemMap item_store;

    insert_item(42, 42, policy, item_store);
    insert_item(18, 18, policy, item_store);
    insert_item(1337, 1337, policy, item_store);

    // At this point, the policy contains [1337, 18, 42] (hottest to coldest).
    // Registering a cache hit on 1337 should not change the ordering of the policy.
    policy.on_cache_hit(item_store.find(1337)->second);

    std::vector<int32_t> victims;
    for (auto i = 0; i < 3; ++i) {
        auto victim = *policy.victim_begin();
        policy.on_evict(victim);
        victims.push_back(victim);
    }

    const std::vector<int32_t> expected_victims{42, 18, 1337};
    EXPECT_EQ(expected_victims, victims);
}

TEST(EvictionLRU, EvictionsWithReordering)
{
    TestLRU policy;

    ItemMap item_store;

    insert_item(42, 42, policy, item_store);
    insert_item(18, 18, policy, item_store);
    insert_item(1337, 1337, policy, item_store);

    policy.on_cache_hit(item_store.find(42)->second);

    std::vector<int32_t> victims;
    for (auto i = 0; i < 3; ++i) {
        auto victim = *policy.victim_begin();
        policy.on_evict(victim);
        victims.push_back(victim);
    }

    const std::vector<int32_t> expected_victims{18, 1337, 42};
    EXPECT_EQ(expected_victims, victims);
}
