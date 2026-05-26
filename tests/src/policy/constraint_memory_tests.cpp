#include <gtest/gtest.h>

#include <absl/hash/hash.h>

#include "cachemere/item.h"
#include "cachemere/policy/constraint_memory.h"

using namespace cachemere;

using TestItem       = Item<uint32_t>;
using TestConstraint = policy::ConstraintMemory<std::string, absl::Hash<std::string>, uint32_t>;

TEST(ConstraintMemory, InitializesMaxMemoryAndMemory)
{
    TestConstraint constraint{10};  // Max 10 bytes.

    EXPECT_EQ(constraint.memory(), 0);
    EXPECT_EQ(constraint.maximum_memory(), 10);
}

TEST(ConstraintMemory, CanAddWhenEnoughRoom)
{
    TestConstraint constraint{10};

    const auto ticket = constraint.prepare_insert("asdf", TestItem{4, 42, 4});
    EXPECT_TRUE(ticket.is_satisfiable());
    EXPECT_TRUE(ticket.is_satisfied());
}

TEST(ConstraintMemory, CanAddWhenFull)
{
    TestConstraint constraint{10};

    constraint.on_insert("asdf", TestItem{5, 42, 5});
    const auto ticket = constraint.prepare_insert("hjkl", TestItem{1, 42, 1});
    EXPECT_TRUE(ticket.is_satisfiable());
    EXPECT_FALSE(ticket.is_satisfied());
}

TEST(ConstraintMemory, CanAddWhenItemTooBig)
{
    TestConstraint constraint{10};

    const auto ticket = constraint.prepare_insert("asdf", TestItem{5, 42, 6});
    EXPECT_FALSE(ticket.is_satisfiable());
    EXPECT_FALSE(ticket.is_satisfied());
}

TEST(ConstraintMemory, CanReplaceWhenEnoughRoom)
{
    TestConstraint constraint{10};
    constraint.on_insert("asdf", TestItem{1, 42, 1});

    EXPECT_EQ(constraint.memory(), 2);

    const auto ticket = constraint.prepare_replace("asdf", TestItem{1, 42, 1}, TestItem{1, 42, 9});
    EXPECT_TRUE(ticket.is_satisfiable());
    EXPECT_TRUE(ticket.is_satisfied());

    constraint.on_update("asdf", TestItem{1, 42, 1}, TestItem{1, 42, 9});

    EXPECT_EQ(constraint.memory(), 10);
}

TEST(ConstraintMemory, CanReplaceWhenItemGrewTooMuch)
{
    TestConstraint constraint{10};
    constraint.on_insert("asdf", TestItem{1, 42, 1});

    const auto ticket = constraint.prepare_replace("asdf", TestItem{1, 42, 1}, TestItem{1, 42, 10});
    EXPECT_FALSE(ticket.is_satisfiable());
    EXPECT_FALSE(ticket.is_satisfied());
}

TEST(ConstraintMemory, CanReplaceWhenShrunk)
{
    TestConstraint constraint{10};
    constraint.on_insert("asdf", TestItem{1, 42, 9});

    const auto ticket = constraint.prepare_replace("asdf", TestItem{1, 42, 9}, TestItem{1, 42, 8});
    EXPECT_TRUE(ticket.is_satisfiable());
    EXPECT_TRUE(ticket.is_satisfied());
}

TEST(ConstraintMemory, OnEvictFreesMemory)
{
    TestConstraint constraint{10};
    constraint.on_insert("asdf", TestItem{1, 42, 9});
    EXPECT_EQ(constraint.memory(), 10);
    constraint.on_evict("asdf", TestItem{1, 42, 9});
    EXPECT_EQ(constraint.memory(), 0);
}

TEST(ConstraintMemory, IsSatisfiedDetectsOverflows)
{
    TestConstraint constraint{10};
    EXPECT_TRUE(constraint.is_satisfied());

    constraint.on_insert("asdf", TestItem{1, 42, 9});
    EXPECT_TRUE(constraint.is_satisfied());

    constraint.update(5);
    EXPECT_FALSE(constraint.is_satisfied());
}
