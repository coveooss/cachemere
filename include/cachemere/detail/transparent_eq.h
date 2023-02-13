#ifndef CACHEMERE_TRANSPARENT_EQ_H
#define CACHEMERE_TRANSPARENT_EQ_H

#include <absl/hash/hash.h>

namespace cachemere::detail {

template<typename Key> struct TransparentEq {
    // Declare this type as transparent - this is needed for abseil maps to support heterogeneous lookup.
    using is_transparent = void;

    bool operator()(const Key& a, const Key& b) const
    {
        return a == b;
    }

    template<typename KeyView> bool operator()(const Key& a, const KeyView& b) const
    {
        // We only require operator==(const Key&) to be defined on the KeyView.
        return b == a;
    }

    template<typename KeyView> bool operator()(const KeyView& a, const Key& b) const
    {
        // We only require operator==(const Key&) to be defined on the KeyView.
        return a == b;
    }
};

}  // namespace cachemere::detail

#endif  // CACHEMERE_TRANSPARENT_EQ_H
