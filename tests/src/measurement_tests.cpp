#include <gtest/gtest.h>

#include <memory>

#include "cachemere/measurement.h"

using namespace cachemere::measurement;

struct Point3D {
    Point3D(uint32_t x, uint32_t y, uint32_t z) : m_x{x}, m_y{y}, m_z{z}
    {
    }

    uint32_t m_x;
    uint32_t m_y;
    uint32_t m_z;

    [[nodiscard]] size_t size() const
    {
        return sizeof(Point3D);
    }
};

TEST(Size, ReferenceSize)
{
    Point3D pt{1, 1, 1};
    ASSERT_EQ(Size<Point3D>()(pt), pt.size());
}

TEST(Size, SharedPointerSize)
{
    auto pt = std::make_shared<Point3D>(1, 1, 1);
    ASSERT_EQ(Size<Point3D>()(pt), pt->size());
}

TEST(Size, UniquePointerSize)
{
    auto pt = std::make_unique<Point3D>(1, 1, 1);
    ASSERT_EQ(Size<Point3D>()(pt), pt->size());
}

struct Container {
    Container(size_t value) : m_value{value}
    {
    }
    [[nodiscard]] size_t capacity() const
    {
        return m_value;
    }

    size_t m_value;
};

void test_capacity(Container container, size_t expected_capacity)
{
    // Test ref.
    ASSERT_EQ(CapacityDynamicallyAllocated<Container>()(container), expected_capacity);

    // Shared ptr.
    auto shared = std::make_shared<Container>(container);
    ASSERT_EQ(CapacityDynamicallyAllocated<Container>()(shared), expected_capacity);

    // Unique ptr
    auto unique = std::make_unique<Container>(container);
    ASSERT_EQ(CapacityDynamicallyAllocated<Container>()(unique), expected_capacity);
}

TEST(CapacityDynamicallyAllocated, OverMaximumRoundingSize)
{
    Container c{1025};
    test_capacity(c, 1025);
}

TEST(CapacityDynamicallyAllocated, SizeAtLeast16)
{
    Container c{0};
    test_capacity(c, 16);
}

TEST(CapacityDynamicallyAllocated, RoundingNoOp)
{
    Container c{2 * sizeof(void*)};
    test_capacity(c, 2 * sizeof(void*));
}

TEST(CapacityDynamicallyAllocated, RoundingToNearestPointerSize)
{
    Container c{2 * sizeof(void*) + 1};
    test_capacity(c, 3 * sizeof(void*));
}
