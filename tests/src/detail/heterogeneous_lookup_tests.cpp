#include <absl/hash/hash.h>
#include <gtest/gtest.h>

#include "cachemere/detail/heterogeneous_lookup.h"

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

TEST(VariadicHash, CanHashSingleType)
{
    using NonRecursiveHasher = VariadicHash<std::string, absl::Hash<std::string>>;

    NonRecursiveHasher hasher;

    hasher("asdf");
}

TEST(VariadicHash, CanHashMultipleTypes)
{
    using RecursiveHasher = VariadicHash<std::string, absl::Hash<std::string>, uint32_t, absl::Hash<uint32_t>>;

    RecursiveHasher hasher;

    hasher("asdf");
    hasher(static_cast<uint32_t>(42));
}