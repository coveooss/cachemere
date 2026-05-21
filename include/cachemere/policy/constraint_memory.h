#pragma once

#include <cassert>
#include <functional>

#include <cachemere/item.h>

namespace cachemere::policy {

/// @brief Memory constraint.
/// @details Use this when the constraint of the cache should be how many bytes of memory it uses.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
template<typename Key, typename KeyHash, typename Value> class ConstraintMemory
{
    using CacheItem = Item<Value>;

public:
    class InsertionTicket
    {
        friend class ConstraintMemory;

    public:
        [[nodiscard]] bool is_satisfiable() const
        {
            return m_satisfiable;
        }

        [[nodiscard]] bool is_satisfied() const
        {
            return m_memory_freed >= m_memory_to_free;
        }

        void register_eviction([[maybe_unused]] const Key& key, const CacheItem& item)
        {
            m_memory_freed += item.m_total_size;
        }

    private:
        InsertionTicket(size_t memory_to_free, bool satisfiable) : m_memory_to_free{memory_to_free}, m_satisfiable{satisfiable}
        {
        }

        size_t m_memory_to_free;
        bool   m_satisfiable;
        size_t m_memory_freed{};
    };

    class ReplacementTicket
    {
        friend class ConstraintMemory;

    public:
        [[nodiscard]] bool is_satisfiable() const
        {
            return m_satisfiable;
        }

        [[nodiscard]] bool is_satisfied() const
        {
            const size_t required_memory_to_free = m_evicted_original_key ? m_memory_to_free_with_insert : m_memory_to_free_with_replace;
            return m_memory_freed >= required_memory_to_free;
        }

        void register_eviction(const Key& key, const CacheItem& item)
        {
            if (!m_evicted_original_key && key == m_original_key.get()) {
                m_evicted_original_key = true;
            }

            m_memory_freed += item.m_total_size;
        }

    private:
        ReplacementTicket(const Key& original_key, size_t memory_to_free_with_replace, size_t memory_to_free_with_insert, bool satisfiable)
         : m_original_key{original_key},
           m_memory_to_free_with_replace{memory_to_free_with_replace},
           m_memory_to_free_with_insert{memory_to_free_with_insert},
           m_satisfiable{satisfiable}
        {
        }

        std::reference_wrapper<const Key> m_original_key;
        size_t                            m_memory_to_free_with_replace;
        size_t                            m_memory_to_free_with_insert;
        bool                              m_satisfiable;
        bool                              m_evicted_original_key{};
        size_t                            m_memory_freed{};
    };

    /// @brief Constructor.
    /// @param max_memory The maximum amount of memory to be used by the cache, in bytes.
    explicit ConstraintMemory(size_t max_memory)
    {
        update(max_memory);
    }

    /// @brief Clears the policy.
    void clear()
    {
        m_memory = 0;
    }

    /// @brief Determines whether an insertion candidate can be added into the cache.
    /// @details That is, whether the constraint would still be satisfied after inserting the candidate.
    /// @param key The key of the insertion candidate.
    /// @param item The candidate item.
    /// @return Whether the item can be added in cache.
    [[nodiscard]] InsertionTicket prepare_insert(const Key& /* key */, const CacheItem& item)
    {
        const size_t memory_to_free = (m_memory + item.m_total_size > m_maximum_memory) ? ((m_memory + item.m_total_size) - m_maximum_memory) : 0;
        return InsertionTicket{memory_to_free, item.m_total_size <= m_maximum_memory};
    }

    /// @brief Determines how much memory would need to be freed to insert a candidate.
    /// @details The returned ticket can be updated with evictions until it becomes satisfied.
    /// @param key The key of the insertion candidate.
    /// @param old_item The current value of the key in cache.
    /// @param new_item The value that would replace the current value.
    /// @return A ticket describing whether the replacement can be satisfied.
    [[nodiscard]] ReplacementTicket prepare_replace(const Key& key, const CacheItem& old_item, const CacheItem& new_item)
    {
        assert(old_item.m_key_size == new_item.m_key_size);  // Key size *really* shouldn't have changed since the key is supposed to be const.

        const size_t memory_to_free_with_replace = ((m_memory - old_item.m_value_size) + new_item.m_value_size > m_maximum_memory)
                                                       ? (((m_memory - old_item.m_value_size) + new_item.m_value_size) - m_maximum_memory)
                                                       : 0;

        const size_t memory_to_free_with_insert =
            (m_memory + new_item.m_total_size > m_maximum_memory) ? ((m_memory + new_item.m_total_size) - m_maximum_memory) : 0;

        return ReplacementTicket{key, memory_to_free_with_replace, memory_to_free_with_insert, new_item.m_total_size <= m_maximum_memory};
    }

    /// @brief Returns whether the constraint is satisfied.
    /// @details Used by the cache after a constraint update to compute how many items should be evicted, if any.
    /// @return Whether the cache constraint is satisfied.
    [[nodiscard]] bool is_satisfied()
    {
        return m_memory <= m_maximum_memory;
    }

    /// @brief Update the cache constraint.
    /// @details Sets a new maximum amount of memory.
    /// @param max_memory The new maximum amount of memory to be used by the cache.
    void update(size_t max_memory)
    {
        m_maximum_memory = max_memory;
    }

    /// @brief Get the amount of memory currently used by the cache.
    /// @return The current amount of memory used, in bytes.
    [[nodiscard]] size_t memory() const
    {
        return m_memory;
    }

    /// @brief Get the maximum amount of memory that can be used by the cache.
    /// @return The maximum amount of memory, in bytes.
    [[nodiscard]] size_t maximum_memory() const
    {
        return m_maximum_memory;
    }

    /// @brief Insertion event handler.
    /// @details Adds the size of this item to the amount of memory used.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& /* key */, const CacheItem& item)
    {
        m_memory += item.m_total_size;
        assert(m_memory <= m_maximum_memory);
    }

    /// @brief Update event handler.
    /// @details Updates the amount of memory used to reflect the size difference between the old and the new value.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key
    void on_update(const Key& /* key */, const CacheItem& old_item, const CacheItem& new_item)
    {
        m_memory -= old_item.m_value_size;
        m_memory += new_item.m_value_size;
        assert(m_memory <= m_maximum_memory);
    }

    /// @brief Eviction event handler.
    /// @details Removes the item at the back of the list - ensuring it has the provided key.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& /* key */, const CacheItem& item)
    {
        assert(item.m_key_size + item.m_value_size <= m_memory);
        m_memory -= item.m_total_size;
    }

private:
    size_t m_maximum_memory;
    size_t m_memory = 0;
};

}  // namespace cachemere::policy
