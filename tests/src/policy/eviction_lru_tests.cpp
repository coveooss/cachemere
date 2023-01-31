#include <gtest/gtest.h>

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include <absl/hash/hash.h>

#include "cachemere/item.h"
#include "cachemere/policy/eviction_lru.h"

using namespace cachemere;

using TestLRU  = policy::EvictionLRU<std::string, absl::Hash<std::string>, int32_t>;
using TestItem = Item<int32_t>;
using ItemMap  = std::map<std::string, TestItem>;

void insert_item(std::string key, int32_t value, TestLRU& policy, ItemMap& item_map)
{
    const auto key_and_item =
        item_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(sizeof(int32_t), value, sizeof(int32_t))).first;

    policy.on_insert(key_and_item->first, key_and_item->second);
}

void expect_victims(const TestLRU& policy, const std::vector<std::string>& expected_victims)
{
    std::vector<std::string> victims;
    for (auto it = policy.victim_begin(); it != policy.victim_end(); ++it) {
        victims.push_back(*it);
    }
    EXPECT_EQ(victims, expected_victims);
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
    auto key_and_item = item_store.find("c");
    policy.on_cache_hit(key_and_item->first, key_and_item->second);

    expect_victims(policy, {"a", "b", "c"});
}

TEST(EvictionLRU, EvictionsWithReordering)
{
    TestLRU policy;

    ItemMap item_store;

    insert_item("a", 42, policy, item_store);
    insert_item("b", 18, policy, item_store);
    insert_item("c", 1337, policy, item_store);

    auto key_and_item = item_store.find("a");
    policy.on_cache_hit(key_and_item->first, key_and_item->second);

    expect_victims(policy, {"b", "c", "a"});
}
