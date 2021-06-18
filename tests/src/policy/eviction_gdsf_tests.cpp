#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>

#include "cachemere/policy/eviction_gdsf.h"

using namespace cachemere;

using TestItem = Item<int32_t>;
using ItemMap  = std::map<std::string, TestItem>;

struct ConstantCost {
    uint32_t operator()(const std::string& /* key */, const TestItem& /* item */)
    {
        return 42;
    }
};

struct QuadraticSizeCost {
    uint32_t operator()(const std::string& /* key */, const TestItem& item)
    {
        return item.m_total_size * item.m_total_size;
    }
};

using ConstantCostGDSF  = policy::EvictionGDSF<std::string, int32_t, ConstantCost>;
using QuadraticCostGDSF = policy::EvictionGDSF<std::string, int32_t, QuadraticSizeCost>;

template<typename Policy> void insert_item(std::string key, int32_t value, Policy& policy, ItemMap& item_map)
{
    const auto key_and_item =
        item_map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(key.capacity(), value, sizeof(int32_t))).first;

    policy.on_insert(key_and_item->first, key_and_item->second);
}

TEST(EvictionGDSF, MaximizesCostPerByteWithConstantCost)
{
    ConstantCostGDSF policy;

    // Policies store references - we need to keep the values alive ourselves.
    ItemMap item_store;

    const std::string short_key = "a";
    const std::string long_key  = "this is supposed to be a much longer string";

    insert_item(short_key, 42, policy, item_store);
    insert_item(long_key, 42, policy, item_store);

    // Since GDSF gives priority to items with a higher cost per byte, using a constant cost favors small items.
    // This means that short_key should be favored over long_key
    EXPECT_EQ(*policy.victim_begin(), long_key);

    for (size_t i = 0; i < 10; ++i) {
        auto key_and_item = item_store.find(long_key);
        policy.on_update(key_and_item->first, key_and_item->second);
    }

    // GDSF does take frequency into account, so touching "this is supposed to be a much longer string" a few times gives it priority again.
    EXPECT_EQ(*policy.victim_begin(), short_key);

    // But since cost/byte is favored, we can load "a" fewer times and get it to stay in cache
    for (size_t i = 0; i < 4; ++i) {
        auto key_and_item = item_store.find(short_key);
        policy.on_update(key_and_item->first, key_and_item->second);
    }

    EXPECT_EQ(*policy.victim_begin(), long_key);
}

TEST(EvictionGDSF, MaximizeCostPerByteWithQuadraticCost)
{
    QuadraticCostGDSF policy;

    // Policies store references - we need to keep the values alive ourselves.
    ItemMap item_store;

    const std::string short_key = "a";
    const std::string long_key  = "this is supposed to be a much longer string";

    insert_item(short_key, 42, policy, item_store);
    insert_item(long_key, 42, policy, item_store);

    // Here GDSF still tries to favor cost/byte, but in this scenario cost increases exponentially with size.
    // This means that bigger items are highly favored in cases where the cost of a cache miss grows
    // heavily with size.
    EXPECT_EQ(*policy.victim_begin(), short_key);

    // We can demonstrate this by accessing the short key more than the long key. The bigger item will still be favored.
    for (size_t i = 0; i < 10; ++i) {
        auto key_and_item = item_store.find(short_key);
        policy.on_update(key_and_item->first, key_and_item->second);
    }

    for (size_t i = 0; i < 4; ++i) {
        auto key_and_item = item_store.find(long_key);
        policy.on_update(key_and_item->first, key_and_item->second);
    }
    EXPECT_EQ(*policy.victim_begin(), short_key);
}

TEST(EvictionGDSF, VictimIteration)
{
    QuadraticCostGDSF policy;

    ItemMap item_store;

    const std::vector<std::string> keys{"a", "b", "c", "d", "e"};
    for (size_t i = 0; i < keys.size(); ++i) {
        insert_item(keys[i], static_cast<int32_t>(i), policy, item_store);
    }

    const std::set<std::string> expected_key_set{keys.begin(), keys.end()};
    std::set<std::string>       key_set;
    for (auto it = policy.victim_begin(); it != policy.victim_end(); ++it) {
        key_set.insert(*it);
    }

    EXPECT_EQ(key_set, expected_key_set);
}
