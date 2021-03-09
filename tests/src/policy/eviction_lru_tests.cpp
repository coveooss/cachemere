#include <gtest/gtest.h>

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "cachemere/detail/item.h"
#include "cachemere/policy/eviction_lru.h"

using namespace cachemere;

using TestLRU  = policy::EvictionLRU<std::string, int32_t>;
using TestItem = detail::Item<std::string, int32_t>;
using ItemMap  = std::map<std::string, TestItem>;

void insert_item(std::string key, int32_t value, TestLRU& policy, ItemMap& item_map)
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

    insert_item("a", 42, policy, item_store);
    insert_item("b", 18, policy, item_store);
    insert_item("c", 1337, policy, item_store);

    const auto victim = *policy.victim_begin();
    policy.on_evict(victim);

    EXPECT_EQ("a", victim);
}

TEST(EvictionLRU, NoOpReordering)
{
    TestLRU policy;

    ItemMap item_store;

    insert_item("a", 42, policy, item_store);
    insert_item("b", 18, policy, item_store);
    insert_item("c", 1337, policy, item_store);

    // At this point, the policy contains [1337, 18, 42] (hottest to coldest).
    // Registering a cache hit on 1337 should not change the ordering of the policy.
    policy.on_cache_hit(item_store.find("c")->second);

    std::vector<std::string> victims;
    for (auto i = 0; i < 3; ++i) {
        auto victim = *policy.victim_begin();
        policy.on_evict(victim);
        victims.push_back(victim);
    }

    const std::vector<std::string> expected_victims{"a", "b", "c"};
    EXPECT_EQ(expected_victims, victims);
}

TEST(EvictionLRU, EvictionsWithReordering)
{
    TestLRU policy;

    ItemMap item_store;

    insert_item("a", 42, policy, item_store);
    insert_item("b", 18, policy, item_store);
    insert_item("c", 1337, policy, item_store);

    policy.on_cache_hit(item_store.find("a")->second);

    std::vector<std::string> victims;
    for (auto i = 0; i < 3; ++i) {
        auto victim = *policy.victim_begin();
        policy.on_evict(victim);
        victims.push_back(victim);
    }

    const std::vector<std::string> expected_victims{"b", "c", "a"};
    EXPECT_EQ(expected_victims, victims);
}
