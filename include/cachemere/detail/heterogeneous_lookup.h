#ifndef CACHEMERE_HETEROGENEOUS_LOOKUP_H
#define CACHEMERE_HETEROGENEOUS_LOOKUP_H

#include <absl/hash/hash.h>

namespace cachemere::detail {

template<typename Key> struct TransparentEq {
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

/// Takes pairs of template parameters of the form [(T, Hash<T>), (U, Hash<U>), ...] and generates an override of the call operator to hash that particular
/// type using its hasher.
template<typename Key, typename KeyHash, typename... Tail> struct VariadicHash;

// Base case.
template<typename Key, typename KeyHash> struct VariadicHash<Key, KeyHash> {
    using is_transparent = void;

    size_t operator()(const Key& key) const
    {
        return m_hash(key);
    }

private:
    KeyHash m_hash;
};

// Recursive case.
template<typename Key, typename KeyHash, typename... Tail> struct VariadicHash : public VariadicHash<Key, KeyHash>, public VariadicHash<Tail...> {
    using is_transparent = void;
};

}  // namespace cachemere::detail

#endif  // CACHEMERE_HETEROGENEOUS_LOOKUP_H
