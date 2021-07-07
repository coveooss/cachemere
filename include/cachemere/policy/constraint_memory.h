#ifndef CACHEMERE_CONSTRAINT_MEMORY_H
#define CACHEMERE_CONSTRAINT_MEMORY_H

#include "cachemere/item.h"

namespace cachemere::policy {

/// @brief Memory constraint.
/// @details Use this when the constraint of the cache should be how many bytes of memory it uses.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam Value The type of the values stored in the cache.
template<typename Key, typename Value> class ConstraintMemory
{
    using CacheItem = Item<Value>;

public:
    /// @brief Constructor.
    /// @param max_memory The maximum amount of memory to be used by the cache, in bytes.
    explicit ConstraintMemory(size_t max_memory);

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

    /// @brief Returns whether a key being looked up in cache still satisfies the constraint.
    /// @details For this constraint, can_find always returns true because an item cannot become invalidated.
    /// @param key The key of the insertion candidate.
    /// @return Whether the key is still valid.
    [[nodiscard]] bool is_invalidated(const Key& key);

    /// @brief Returns whether the constraint is satisfied.
    /// @details Used by the cache after a constraint update to compute how many items should be evicted, if any.
    /// @return Whether the cache constraint is satisfied.
    [[nodiscard]] bool is_satisfied();

    /// @brief Update the cache constraint.
    /// @details Sets a new maximum amount of memory.
    /// @param max_memory The new maximum amount of memory to be used by the cache.
    void update(size_t max_memory);

    /// @brief Get the amount of memory currently used by the cache.
    /// @return The current amount of memory used, in bytes.
    [[nodiscard]] size_t memory() const;

    /// @brief Get the maximum amount of memory that can be used by the cache.
    /// @return The maximum amount of memory, in bytes.
    [[nodiscard]] size_t maximum_memory() const;

    /// @brief Insertion event handler.
    /// @details Adds the size of this item to the amount of memory used.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& key, const CacheItem& item);

    /// @brief Update event handler.
    /// @details Updates the amount of memory used to reflect the size difference between the old and the new value.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key
    void on_update(const Key& key, const CacheItem& old_item, const CacheItem& new_item);

    /// @brief Eviction event handler.
    /// @details Removes the item at the back of the list - ensuring it has the provided key.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& key, const CacheItem& item);

private:
    size_t m_maximum_memory;
    size_t m_memory = 0;
};

}  // namespace cachemere::policy

#include "constraint_memory.hpp"

#endif
