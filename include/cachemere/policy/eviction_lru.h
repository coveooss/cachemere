#pragma once

#include <cassert>
#include <functional>
#include <list>

#include <absl/container/flat_hash_map.h>

#include <cachemere/concepts.h>
#include <cachemere/item.h>

namespace cachemere::policy {

/// @brief Least Recently Used (LRU) eviction policy.
/// @details Implemented internally using a linked list.
///          The keys are ordered from most-recently used to least-recently used.
///          Only stores references to keys kept alive by the cache.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
template<CacheKey Key, HasherFor<Key> KeyHash, typename Value> class EvictionLRU
{
private:
    using KeyRef    = std::reference_wrapper<const Key>;
    using KeyRefIt  = typename std::list<KeyRef>::iterator;
    using KeyRefMap = absl::flat_hash_map<KeyRef, KeyRefIt, KeyHash, std::equal_to<Key>>;

public:
    using CacheItem = cachemere::Item<Value>;

    /// @brief Ticket describing an item tentatively removed from the policy.
    /// @details Tickets let the cache roll back provisional evictions if an insertion is later rejected.
    struct Ticket {
        explicit Ticket(KeyRef key) : m_key{key}
        {
        }

        /// @brief Get the key of the tentatively evicted item.
        /// @return The cached key reference.
        [[nodiscard]] const Key& key() const
        {
            return m_key;
        }

        KeyRef m_key;
    };

    /// @brief Clears the policy.
    void clear()
    {
        m_keys.clear();
        m_nodes.clear();
    }

    /// @brief Insertion event handler.
    /// @details Inserts the provided item at the front of the list.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& key, const CacheItem& /* item */)
    {
        assert(m_nodes.find(std::ref(key)) == m_nodes.end());  // Validate the item is not already in policy.
        m_keys.emplace_front(std::ref(key));
        m_nodes.emplace(std::ref(key), m_keys.begin());
    }

    /// @brief Update event handler.
    /// @details Moves the provided item to the front of the list.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key
    void on_update(const Key& key, const CacheItem& /* old_item */, const CacheItem& new_item)
    {
        on_cache_hit(key, new_item);
    }

    /// @brief Cache hit event handler.
    /// @details Moves the provided item at the front of the list.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& /* item */)
    {
        auto node_it = m_nodes.find(key);
        if (node_it != m_nodes.end()) {
            // No need to shuffle stuff around if item is already the hottest item in cache.
            if (node_it->second != m_keys.begin()) {
                m_keys.splice(m_keys.begin(), m_keys, node_it->second);
            }
        } else {
            // If this is tripped, there is a disconnect between the contents of the policy and the contents of the cache.
            assert(false);
        }
    }

    /// @brief Eviction event handler.
    /// @details Removes the item at the back of the list - ensuring it has the provided key.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& key, const CacheItem& /* item */)
    {
        assert(!m_nodes.empty());
        assert(!m_keys.empty());

        if (m_keys.back().get() == key) {
            m_nodes.erase(key);
            m_keys.pop_back();
        } else {
            auto it = m_nodes.find(key);
            assert(it != m_nodes.end());
            m_keys.erase(it->second);
            m_nodes.erase(it);
        }
    }

    /// @brief Remove and return the next victim according to the LRU ordering.
    /// @return A ticket that can later be committed by the caller or restored with `rollback()`.
    [[nodiscard]] Ticket pop_victim()
    {
        assert(!m_keys.empty());

        const KeyRef victim_key = m_keys.back();
        m_nodes.erase(victim_key);
        m_keys.pop_back();
        return Ticket{victim_key};
    }

    /// @brief Restore a victim previously returned by `pop_victim()`.
    /// @param ticket The ticket describing the victim to restore.
    void rollback(Ticket ticket)
    {
        assert(m_nodes.find(ticket.m_key) == m_nodes.end());

        m_keys.emplace_back(ticket.m_key);
        m_nodes.emplace(ticket.m_key, std::prev(m_keys.end()));
    }

private:
    std::list<KeyRef> m_keys;
    KeyRefMap         m_nodes;
};

}  // namespace cachemere::policy
