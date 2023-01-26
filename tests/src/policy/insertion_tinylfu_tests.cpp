#include <gtest/gtest.h>

#include "cachemere/policy/insertion_tinylfu.h"

using namespace cachemere;

using TestPolicy = policy::InsertionTinyLFU<uint32_t, std::hash<uint32_t>, uint32_t>;

TEST(InsertionTinyLFU, ShouldAddAlwaysTrue)
{
    TestPolicy policy;
    for (auto i = 0; i < 100; ++i) {
        policy.on_cache_miss(i);  // Key needs to be touched once before being inserted.
        EXPECT_TRUE(policy.should_add(i));
    }
}

TEST(InsertionTinyLFU, ReplacementPreferenceBasic)
{
    TestPolicy policy;

    for (auto i = 0; i < 10; ++i) {
        policy.on_cache_miss(42);
    }

    for (auto i = 0; i < 5; ++i) {
        policy.on_cache_miss(18);
    }

    EXPECT_TRUE(policy.should_replace(18, 42));
}

TEST(InsertionTinyLFU, ResetWhenReachedCardinality)
{
    TestPolicy policy;
    policy.set_cardinality(5);

    policy.on_cache_miss(3);
    policy.on_cache_miss(3);

    // After this loop, the counter value will be at the maximal value it can be without
    // resetting.
    for (auto i = 0; i < 6; ++i) {
        policy.on_cache_miss(42);
    }

    // This means that the policy will recommend replacing 3 by 42.
    EXPECT_TRUE(policy.should_replace(3, 42));

    // If we touch the item one more time (for a total of 7), we should trigger a reset.
    // (We touch item 7 times despite the cardinality of the set being 5.
    // This is because the resize is triggered once the count becomes *bigger* than the cardinality,
    // and the first access is blocked by the gatekeeper bloom filter.)
    policy.on_cache_miss(42);

    // If a reset was performed, the policy should have divided the counts by two,
    // bringing the count for `42` from 6 to 3, and the count for `3` from 1 to 0.
    // This means that touching `3` four more times would make the policy recommend
    // replacing 42 by 3.
    EXPECT_FALSE(policy.should_replace(42, 3));
    for (auto i = 0; i < 4; ++i) {
        policy.on_cache_miss(3);
    }
    EXPECT_TRUE(policy.should_replace(42, 3));
}

TEST(InsertionTinyLFU, ResetLeavesNonZeroValues)
{
    TestPolicy policy;
    policy.set_cardinality(5);

    policy.on_cache_miss(3);
    policy.on_cache_miss(3);

    // Touch 42 until we trigger a reset.
    for (auto i = 0; i < 7; ++i) {
        policy.on_cache_miss(42);
    }

    policy.on_cache_miss(1);

    // If the reset left a non-zero counter value, the policy should still prefer 42 over 1.
    EXPECT_FALSE(policy.should_replace(42, 1));
}
