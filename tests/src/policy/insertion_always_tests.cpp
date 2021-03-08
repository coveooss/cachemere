#include <gtest/gtest.h>

#include "cachemere/detail/item.h"
#include "cachemere/policy/insertion_always.h"

using namespace cachemere::policy;

TEST(InsertionAlways, AlwaysInserts)
{
    InsertionAlways<uint32_t, uint32_t> policy;

    for (auto i = 0; i < 100; ++i) {
        EXPECT_TRUE(policy.should_add(i));
    }
}

TEST(InsertionAlways, AlwaysReplaces)
{
    InsertionAlways<uint32_t, uint32_t> policy;

    for (auto i = 1; i < 100; ++i) {
        EXPECT_TRUE(policy.should_replace(i - 1, i));
    }
}
