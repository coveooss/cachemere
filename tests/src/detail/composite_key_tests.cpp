#include <string>
#include <string_view>

#include <absl/hash/hash.h>
#include <gtest/gtest.h>

#include <cachemere.h>

namespace {

using namespace cachemere;

TEST(CompositeKey, CanConstructFromLvalueStrings)
{
    std::string first{"alpha"};
    std::string second{"beta"};

    CompositeKey<std::string, std::string>           key{first, second};
    CompositeKey<std::string_view, std::string_view> view_key{first, second};

    EXPECT_TRUE(key == view_key);
}

TEST(CompositeKey, CanCompareDifferentSpecializations)
{
    CompositeKey<std::string, std::string>           owned_key{"alpha", "beta"};
    CompositeKey<std::string_view, std::string_view> same_view_key{"alpha", "beta"};
    CompositeKey<std::string_view, std::string_view> other_view_key{"alpha", "gamma"};

    EXPECT_TRUE(owned_key == same_view_key);
    EXPECT_TRUE(same_view_key == owned_key);
    EXPECT_FALSE(owned_key == other_view_key);
    EXPECT_FALSE(other_view_key == owned_key);
}

TEST(CompositeKey, HashesStringsAndStringViewsTheSame)
{
    using OwnedKey = CompositeKey<std::string, std::string, std::uint32_t>;
    using ViewKey  = CompositeKey<std::string_view, std::string_view, std::uint32_t>;

    OwnedKey owned_key{"alpha", "beta", 42U};
    ViewKey  view_key{"alpha", "beta", 42U};

    EXPECT_EQ(absl::Hash<OwnedKey>{}(owned_key), absl::Hash<ViewKey>{}(view_key));
}

TEST(CompositeKey, SupportsHeterogeneousLookup)
{
    using OwnedKey = CompositeKey<std::string, std::string>;
    using ViewKey  = CompositeKey<std::string_view, std::string_view>;
    using Hasher   = MultiHash<OwnedKey, absl::Hash<OwnedKey>, ViewKey, absl::Hash<ViewKey>>;
    using Cache    = presets::memory::LRUCache<OwnedKey, std::uint32_t, measurement::SizeOf<std::uint32_t>, measurement::SizeOf<OwnedKey>, Hasher>;

    Cache cache{32U * sizeof(std::uint32_t)};

    cache.insert(OwnedKey{"alpha", "beta"}, 7U);

    auto value = cache.find(ViewKey{"alpha", "beta"});

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 7U);
    EXPECT_TRUE(cache.contains(ViewKey{"alpha", "beta"}));
    EXPECT_FALSE(cache.contains(ViewKey{"alpha", "gamma"}));
}

}  // namespace
