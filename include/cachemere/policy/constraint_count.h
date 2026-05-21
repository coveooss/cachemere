#pragma once

#include <cassert>

#include <cachemere/item.h>
#include <endian.h>

namespace cachemere::policy {

/// @brief Count constraint.
/// @details Use this when the constraint of the cache should be the number of items in cache.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
template<typename Key, typename KeyHash, typename Value> class ConstraintCount
{
    using CacheItem = Item<Value>;

public:
    class InsertionTicket
    {
        friend class ConstraintCount;

    public:
        [[nodiscard]] bool is_satisfiable() const
        {
            return m_satisfiable;
        }

        [[nodiscard]] bool is_satisfied() const
        {
            return m_count_freed >= m_count_to_free;
        }

        void register_eviction([[maybe_unused]] const Key& key, [[maybe_unused]] const CacheItem& item)
        {
            ++m_count_freed;
        }

    private:
        InsertionTicket(size_t count_to_free, bool satisfiable) : m_count_to_free{count_to_free}, m_satisfiable{satisfiable}
        {
        }

        size_t m_count_to_free;
        bool   m_satisfiable;
        size_t m_count_freed{};
    };

    class ReplacementTicket
    {
    public:
        [[nodiscard]] bool is_satisfiable() const
        {
            return true;
        }

        [[nodiscard]] bool is_satisfied() const
        {
            return true;
        }

        void register_eviction([[maybe_unused]] const Key& key, [[maybe_unused]] const CacheItem& item)
        {
        }
    };

    explicit ConstraintCount(size_t maximum_count)
    {
        update(maximum_count);
    }

    /// @brief Clears the policy.
    void clear()
    {
        m_count = 0;
    }

    [[nodiscard]] InsertionTicket prepare_insert(const Key& /* key */, const CacheItem& /* item */)
    {
        const size_t count_to_free = (m_count + 1 > m_maximum_count) ? ((m_count + 1) - m_maximum_count) : 0;
        return InsertionTicket{count_to_free, m_maximum_count > 0};
    }

    [[nodiscard]] ReplacementTicket prepare_replace(const Key& /* key */, const CacheItem& /* old_item */, const CacheItem& /* new_item */)
    {
        return ReplacementTicket{};
    }

    /// @brief Returns whether the constraint is satisfied.
    /// @details Used by the cache after a constraint update to compute how many items should be evicted, if any.
    /// @return Whether the cache constraint is satisfied.
    [[nodiscard]] bool is_satisfied()
    {
        return m_count <= m_maximum_count;
    }

    /// @brief Update the cache constraint.
    /// @details Sets a new maximum count.
    /// @param maximum_count The new number of items in cache.
    void update(size_t maximum_count)
    {
        m_maximum_count = maximum_count;
    }

    /// @brief Insertion event handler.
    /// @details Adds one to the number of items in cache.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& /* key */, const CacheItem& /* item */)
    {
        ++m_count;
    }

    /// @brief Eviction event handler.
    /// @details Removes one from the number of items in cache.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& /* key */, const CacheItem& /* item */)
    {
        assert(m_count > 0);
        --m_count;
    }

    /// @brief Get the number of items currently in the cache.
    /// @return The number of items in cache.
    [[nodiscard]] size_t count() const
    {
        return m_count;
    }

    /// @brief Get the maximum number of items allowed in cache.
    /// @return The maximum number of items allowed in cache.
    [[nodiscard]] size_t maximum_count() const
    {
        return m_maximum_count;
    }

private:
    size_t m_maximum_count;
    size_t m_count = 0;
};

}  // namespace cachemere::policy
