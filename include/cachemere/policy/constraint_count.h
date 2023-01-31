#ifndef CACHEMERE_CONSTRAINT_COUNT_H
#define CACHEMERE_CONSTRAINT_COUNT_H

#include "cachemere/item.h"

namespace cachemere::policy {

/// @brief Count constraint.
/// @details Use this when the constraint of the cache should be the number of items in cache.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam Value The type of the values stored in the cache.
template<typename Key, typename KeyHash, typename Value> class ConstraintCount
{
    using CacheItem = Item<Value>;

public:
    explicit ConstraintCount(size_t maximum_count);

    /// @brief Clears the policy.
    void clear();

    /// @brief Determines whether an insertion candidate can be added into the cache.
    /// @details That is, whether the constraint would still be satisfied after inserting the candidate.
    /// @param key The key of the insertion candidate.
    /// @param item The candidate item.
    /// @return Whether the item can be added in cache.
    [[nodiscard]] bool can_add(const Key& key, const CacheItem& item);

    /// @brief Determines whether an item already in cache can be updated.
    /// @details That is, whether the key can be updated to the new value while still satisfying the constraint.
    /// @param key The key to be updated.
    /// @param old_item The current value of the key in cache.
    /// @param new_item The value that would replace the current value.
    /// @return Whether the item can be replaced.
    [[nodiscard]] bool can_replace(const Key& key, const CacheItem& old_item, const CacheItem& new_item);

    /// @brief Returns whether the constraint is satisfied.
    /// @details Used by the cache after a constraint update to compute how many items should be evicted, if any.
    /// @return Whether the cache constraint is satisfied.
    [[nodiscard]] bool is_satisfied();

    /// @brief Update the cache constraint.
    /// @details Sets a new maximum count.
    /// @param maximum_count The new number of items in cache.
    void update(size_t maximum_count);

    /// @brief Insertion event handler.
    /// @details Adds one to the number of items in cache.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& key, const CacheItem& item);

    /// @brief Eviction event handler.
    /// @details Removes one from the number of items in cache.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& key, const CacheItem& item);

    /// @brief Get the number of items currently in the cache.
    /// @return The number of items in cache.
    [[nodiscard]] size_t count() const;

    /// @brief Get the maximum number of items allowed in cache.
    /// @return The maximum number of items allowed in cache.
    [[nodiscard]] size_t maximum_count() const;

private:
    size_t m_maximum_count;
    size_t m_count = 0;
};

}  // namespace cachemere::policy

#include "constraint_count.hpp"

#endif
