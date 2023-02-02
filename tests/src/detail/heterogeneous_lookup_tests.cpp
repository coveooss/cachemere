#include <absl/hash/hash.h>
#include <gtest/gtest.h>

#include "cachemere/hash.h"
#include "cachemere/detail/transparent_eq.h"

using namespace cachemere::detail;

TEST(TransparentEq, CanEqualSelf)
{
    using EqT = TransparentEq<std::string>;

    EqT         eq;
    std::string val{"asdf"};

    EXPECT_TRUE(eq(val, val));
    EXPECT_TRUE(eq(val, std::string_view("asdf")));
    EXPECT_FALSE(eq(val, std::string_view("bing bong")));
    EXPECT_TRUE(eq(val, val.c_str()));
}

TEST(MultiHash, CanHashSingleType)
{
    using NonRecursiveHasher = cachemere::MultiHash<std::string, absl::Hash<std::string>>;

    NonRecursiveHasher hasher;

    hasher("asdf");
}

TEST(MultiHash, CanHashMultipleTypes)
{
    using RecursiveHasher = cachemere::MultiHash<std::string, absl::Hash<std::string>, uint32_t, absl::Hash<uint32_t>>;

    RecursiveHasher hasher;

    hasher("asdf");
    hasher(static_cast<uint32_t>(42));
}

struct CompositeType {
    std::string a;
    std::string b;

    template<typename H> friend H AbslHashValue(H h, const CompositeType& s)
    {
        return H::combine(std::move(h), s.a, s.b);
    }

    bool operator==(const CompositeType& other) const
    {
        return a == other.a && b == other.b;
    }
};

struct CompositeView {
    std::string_view a;
    std::string_view b;

    bool operator==(const CompositeView& other) const
    {
        return a == other.a && b == other.b;
    }

    bool operator==(const CompositeType& other) const
    {
        return a == other.a && b == other.b;
    }

    template<typename H> friend H AbslHashValue(H h, const CompositeView& s)
    {
        return H::combine(std::move(h), s.a, s.b);
    }
};

TEST(MultiHash, CanHashComplexTypes)
{
    using MultiHasher = cachemere::MultiHash<CompositeType, absl::Hash<CompositeType>, CompositeView, absl::Hash<CompositeView>>;

    MultiHasher hasher;

    auto hash_a = hasher(CompositeType{"a", "b"});
    auto hash_b = hasher(CompositeView{std::string_view{"a"}, std::string_view{"b"}});

    EXPECT_EQ(hash_a, hash_b);
}