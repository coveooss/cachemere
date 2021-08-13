#include <gtest/gtest.h>

#include "cachemere/item.h"
#include "cachemere/policy/constraint_count.h"

using namespace cachemere;

using TestItem       = Item<uint32_t>;
using TestConstraint = policy::ConstraintCount<std::string, uint32_t>;

TEST(ConstraintCount, InitializesMaxCountAndCount)
{
    TestConstraint constraint{10};

    EXPECT_EQ(constraint.count(), 0);
    EXPECT_EQ(constraint.maximum_count(), 10);
}

TEST(ConstraintCount, CanAddWhenEnoughRoom)
{
    TestConstraint constraint{2};
    EXPECT_TRUE(constraint.can_add("asdf", TestItem{1, 1, 1}));
}

TEST(ConstraintCount, CanAddWhenFull)
{
    TestConstraint constraint{2};

    for (uint32_t i = 0; i < 2; ++i) {
        constraint.on_insert("asdf", TestItem{i, i, i});
    }

    EXPECT_EQ(constraint.count(), 2);
    EXPECT_FALSE(constraint.can_add("asdf", TestItem{1, 1, 1}));
}

TEST(ConstraintCount, CanReplaceWhenThereIsRoom)
{
    TestConstraint constraint{2};
    constraint.on_insert("asdf", TestItem{1, 1, 1});

    EXPECT_TRUE(constraint.can_replace("asdf", TestItem{1, 1, 1}, TestItem{2, 2, 2}));
}

TEST(ConstraintCount, CanReplaceWhenFull)
{
    TestConstraint constraint{1};
    constraint.on_insert("asdf", TestItem{1, 1, 1});

    EXPECT_TRUE(constraint.can_replace("asdf", TestItem{1, 1, 1}, TestItem{2, 2, 2}));
}

TEST(ConstraintCount, OnEvictDecreasesCount)
{
    TestConstraint constraint{1};
    constraint.on_insert("asdf", TestItem{1, 1, 1});
    EXPECT_EQ(constraint.count(), 1);

    constraint.on_evict("asdf", TestItem{1, 1, 1});
    EXPECT_EQ(constraint.count(), 0);
}

TEST(ConstraintCount, IsSatisfiedDetectsOverflows)
{
    TestConstraint constraint{10};
    EXPECT_TRUE(constraint.is_satisfied());

    for (uint32_t i = 0; i < 10; ++i) {
        constraint.on_insert("asdf", TestItem{i, i, i});
    }

    EXPECT_TRUE(constraint.is_satisfied());

    constraint.update(5);
    EXPECT_FALSE(constraint.is_satisfied());
}
