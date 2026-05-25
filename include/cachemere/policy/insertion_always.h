#pragma once

namespace cachemere::policy {

/// @brief Simplest insertion policy. Always accepts insertions.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
template<typename Key, typename KeyHash, typename Value> class InsertionAlways
{
public:
    /// @brief Clears the policy.
    void clear();

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

template<typename Key, typename KeyHash, typename Value> void InsertionAlways<Key, KeyHash, Value>::clear()
{
}

template<typename Key, typename KeyHash, typename Value> bool InsertionAlways<Key, KeyHash, Value>::should_add(const Key& /* key */)
{
    return true;
}

template<typename Key, typename KeyHash, typename Value>
bool InsertionAlways<Key, KeyHash, Value>::should_replace(const Key& /* key */, const Key& /* candidate */)
{
    return true;
}

}  // namespace cachemere::policy
