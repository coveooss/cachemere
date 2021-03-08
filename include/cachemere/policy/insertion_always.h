#ifndef CACHEMERE_INSERTION_ALWAYS_H
#define CACHEMERE_INSERTION_ALWAYS_H

#include <iostream>

#include "cachemere/detail/item.h"
namespace cachemere::policy {

/// @brief Simplest insertion policy. Always accepts insertions.
template<typename Key, typename Value> class InsertionAlways
{
public:
    /// @brief Determines whether a given key should be inserted into the cache.
    /// @details For this policy, `should_add` always returns true.
    /// @param key The key of the insertion candidate.
    /// @return Whether the candidate should be inserted into the cache.
    bool should_add(const Key& key);

    /// @brief Determines whether a given victim should be replaced by a given candidate.
    /// @details For this policy, `should_replace` always returns true.
    /// @param victim The key of the victim the candidate will be compared to.
    /// @param candidate The key of the insertion candidate.
    /// @return Whether the victim should be replaced by the candidate.
    bool should_replace(const Key& victim, const Key& candidate);
};

}  // namespace cachemere::policy

#include "insertion_always.hpp"

#endif
