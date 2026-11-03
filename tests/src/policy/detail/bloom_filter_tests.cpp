#include <absl/hash/hash.h>
#include <gtest/gtest.h>

#include "cachemere/policy/detail/bloom_filter.h"
#include "cachemere/hash.h"

using namespace cachemere::policy::detail;

TEST(BloomFilter, BasicAdd)
{
    BloomFilter<absl::Hash<std::string>> filter{5};
    filter.add("hello world");
    ASSERT_TRUE(filter.maybe_contains("hello world"));
}

// Unlike `absl::Hash`, which is seeded differently in each process, this hash gives the same false positives on every run.
struct SplitMix64Hash {
    size_t operator()(uint32_t value) const
    {
        uint64_t z = value + 0x9e3779b97f4a7c15ULL;
        z          = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z          = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return static_cast<size_t>(z ^ (z >> 31U));
    }
};

TEST(BloomFilter, FalsePositiveRate)
{
    const uint32_t cardinality = 100;

    BloomFilter<SplitMix64Hash> filter{cardinality};

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
    const uint32_t                    cardinality = 5;
    BloomFilter<absl::Hash<uint32_t>> filter{cardinality};

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
    BloomFilter<absl::Hash<uint32_t>> filter{5};

    filter.add(42);
    EXPECT_TRUE(filter.maybe_contains(42));

    const auto size_pre_clear = filter.memory_used();
    filter.clear();

    EXPECT_FALSE(filter.maybe_contains(42));
    EXPECT_GT(filter.memory_used(), static_cast<size_t>(0));
    EXPECT_LT(abs(static_cast<int32_t>(size_pre_clear) - static_cast<int32_t>(filter.memory_used())), 500);
}

TEST(BloomFilter, HeterogeneousLookupString)
{
    using Hash = cachemere::MultiHash<std::string, absl::Hash<std::string>, std::string_view, absl::Hash<std::string_view>>;

    BloomFilter<Hash> filter{5};

    const std::string      key_str        = "asdf";
    const std::string_view key_view       = "asdf";
    const std::string_view unrelated_view = "hjkl";

    filter.add(key_str);

    EXPECT_TRUE(filter.maybe_contains(key_view));
    EXPECT_FALSE(filter.maybe_contains(unrelated_view));
}