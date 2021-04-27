#include <gtest/gtest.h>

#include "cachemere/policy/detail/bloom_filter.h"

using namespace cachemere::policy::detail;

TEST(BloomFilter, BasicAdd)
{
    BloomFilter<std::string> filter{5};
    filter.add("hello world");
    ASSERT_TRUE(filter.maybe_contains("hello world"));
}

TEST(BloomFilter, FalsePositiveRate)
{
    const uint32_t cardinality = 100;

    BloomFilter<uint32_t> filter{cardinality};

    for (uint32_t i = 0; i < cardinality; ++i) {
        filter.add(i);
    }

    // Validate all items were added to the filter.
    for (uint32_t i = 0; i < cardinality; ++i) {
        EXPECT_TRUE(filter.maybe_contains(i));
    }

    size_t falsePositives = 0;
    for (auto i = cardinality; i < cardinality + 1000; ++i) {
        if (filter.maybe_contains(i)) {
            ++falsePositives;
        }
    }

    const size_t falsePositiveThreshold = 20;  // 2% error.
    EXPECT_LT(falsePositives, falsePositiveThreshold);
}

TEST(BloomFilter, FilterSaturation)
{
    const uint32_t        cardinality = 5;
    BloomFilter<uint32_t> filter{cardinality};

    // Completely saturate the filter. After this, every filter bit should be set to `1`.
    for (uint32_t i = 0; i < cardinality * 100; ++i) {
        filter.add(i);
    }

    EXPECT_EQ(filter.saturation(), 1.0);

    // Which means that querying random numbers should always return true.
    for (auto i = 9000; i < 11000; ++i) {
        EXPECT_TRUE(filter.maybe_contains(i));
    }
}

TEST(BloomFilter, Clear)
{
    BloomFilter<uint32_t> filter{5};

    filter.add(42);
    EXPECT_TRUE(filter.maybe_contains(42));

    const auto size_pre_clear = filter.memory_used();
    filter.clear();

    EXPECT_FALSE(filter.maybe_contains(42));
    EXPECT_GT(filter.memory_used(), static_cast<size_t>(0));
    EXPECT_LT(abs(static_cast<int32_t>(size_pre_clear) - static_cast<int32_t>(filter.memory_used())), 500);
}
