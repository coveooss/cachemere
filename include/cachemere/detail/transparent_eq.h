#pragma once

#include <concepts>

namespace cachemere::detail {

/// @brief Whether two values can be compared for equality, either with `==` or with `<=>`.
template<typename Lhs, typename Rhs>
concept EqualityOrThreeWayComparable = requires(const Lhs& lhs, const Rhs& rhs) {
    { lhs == rhs } -> std::convertible_to<bool>;
} || requires(const Lhs& lhs, const Rhs& rhs) {
    { (lhs <=> rhs) == 0 } -> std::convertible_to<bool>;
};

template<typename Key> struct TransparentEq {
    // Declare this type as transparent - this is needed for abseil maps to support heterogeneous lookup.
    using is_transparent = void;

    bool operator()(const Key& a, const Key& b) const
    {
        return equal(a, b);
    }

    template<typename KeyView>
        requires EqualityOrThreeWayComparable<KeyView, Key>
    bool operator()(const Key& a, const KeyView& b) const
    {
        // We only require equality or three-way comparison against `Key` to be defined on the KeyView.
        return equal(b, a);
    }

    template<typename KeyView>
        requires EqualityOrThreeWayComparable<KeyView, Key>
    bool operator()(const KeyView& a, const Key& b) const
    {
        // We only require equality or three-way comparison against `Key` to be defined on the KeyView.
        return equal(a, b);
    }

private:
    template<typename Lhs, typename Rhs> static bool equal(const Lhs& lhs, const Rhs& rhs)
    {
        if constexpr (requires { lhs == rhs; }) {
            return lhs == rhs;
        } else {
            return (lhs <=> rhs) == 0;
        }
    }
};

}  // namespace cachemere::detail
