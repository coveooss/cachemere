#ifndef CACHEMERE_HASH_H
#define CACHEMERE_HASH_H

#include <cstdint>

namespace cachemere {

/// @brief Allows combining hashers for multiple types into a single hashing object.
/// @details Takes pairs of template parameters of the form [(T, Hash<T>), (U, Hash<U>), ...] and generates an override of the call operator to hash that
///        particular type using its hasher.
template<typename Key, typename KeyHash, typename... Tail> struct MultiHash;

// Base case.
template<typename Key, typename KeyHash> struct MultiHash<Key, KeyHash> {
    // Declare this type as transparent - this is needed for abseil maps to support heterogeneous lookup.
    using is_transparent = void;

    size_t operator()(const Key& key) const
    {
        return m_hash(key);
    }

private:
    KeyHash m_hash;
};

// Recursive case.
template<typename Key, typename KeyHash, typename... Tail> struct MultiHash : public MultiHash<Key, KeyHash>, public MultiHash<Tail...> {
    // Declare this type as transparent - this is needed for abseil maps to support heterogeneous lookup.
    using is_transparent = void;

    // Need to bring back operator() declarations from parent types, so they can all be considered as overloads.
    using MultiHash<Key, KeyHash>::operator();
    using MultiHash<Tail...>::     operator();
};

}  // namespace cachemere

#endif  // CACHEMERE_HASH_H
