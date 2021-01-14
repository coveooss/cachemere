#include <gtest/gtest.h>

#include "cachemere/policy/detail/counting_bloom_filter.h"

using namespace cachemere::policy::detail;

TEST(CountingBloomFilter, BasicCount) {
    CountingBloomFilter<std::string> filter{5};
    EXPECT_EQ(0, filter.estimate("hello world"));

    filter.add("hello world");
    EXPECT_EQ(1, filter.estimate("hello world"));

    filter.add("hello world");
    EXPECT_EQ(2, filter.estimate("hello world"));
}

TEST(CountingBloomFilter, FilterSaturation) {
    CountingBloomFilter<uint32_t> filter{5};

    for (auto i = 0; i < 1000; ++i) {
        filter.add(i);
    }

    EXPECT_EQ(filter.saturation(), 1.0);

    for (auto i = 5000; i < 5500; ++i) {
        EXPECT_GT(filter.estimate(i), static_cast<uint32_t>(0));
    }
}

TEST(CountingBloomFilter, Clear) {
    CountingBloomFilter<uint32_t> filter{5};
    filter.add(42);

    auto sizePreClear = filter.memory_used();

    EXPECT_EQ(1, filter.estimate(42));

    filter.clear();
    EXPECT_EQ(0, filter.estimate(42));
    EXPECT_EQ(sizePreClear, filter.memory_used());
}
