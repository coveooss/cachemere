#pragma once

#include <absl/hash/hash.h>

namespace cachemere::detail {

template<typename Key> struct TransparentEq {
    // Declare this type as transparent - this is needed for abseil maps to support heterogeneous lookup.
    using is_transparent = void;

    bool operator()(const Key& a, const Key& b) const
    {
        return equal(a, b);
    }

    template<typename KeyView> bool operator()(const Key& a, const KeyView& b) const
    {
        // We only require equality or three-way comparison against `Key` to be defined on the KeyView.
        return equal(b, a);
    }

    template<typename KeyView> bool operator()(const KeyView& a, const Key& b) const
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
