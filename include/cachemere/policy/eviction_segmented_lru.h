#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <list>

#include <absl/container/flat_hash_map.h>

#include <cachemere/concepts.h>
#include <cachemere/item.h>

namespace cachemere::policy {

/// @brief Segmented Least Recently Used (S-LRU) Eviction Policy.
/// @details Segmented LRU is really similar to LRU. The main difference is that a segmented LRU
///          policy has _two_ separate LRU segments. Items are initially inserted in a probation segment.
///          When items that are on probation are accessed, they are promoted from the protected segment.
///          If the protected segment is full, the item that was least-recently accessed is downgraded to
///          the probation segment. This architecture has the effect of reducing cache churn and keeping
///          "good" items in cache a bit longer.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
template<CacheKey Key, HasherFor<Key> KeyHash, typename Value> class EvictionSegmentedLRU
{
private:
    using KeyRef    = std::reference_wrapper<const Key>;
    using KeyRefIt  = typename std::list<KeyRef>::iterator;
    using KeyRefMap = absl::flat_hash_map<KeyRef, KeyRefIt, KeyHash, std::equal_to<Key>>;

public:
    using CacheItem = cachemere::Item<Value>;

    /// @brief Ticket describing an item tentatively removed from one of the S-LRU segments.
    /// @details Tickets retain the segment information needed to roll back a provisional eviction.
    struct Ticket {
        Ticket(KeyRef key, bool from_probation) : m_key{key}, m_from_probation{from_probation}
        {
        }

        /// @brief Get the key of the tentatively evicted item.
        /// @return The cached key reference.
        [[nodiscard]] const Key& key() const
        {
            return m_key;
        }

        KeyRef m_key;
        bool   m_from_probation;
    };

    /// @brief Clears the policy.
    void clear()
    {
        m_probation_list.clear();
        m_probation_nodes.clear();

        m_protected_list.clear();
        m_protected_nodes.clear();
    }

    /// @brief Set the maximum number of items in the protected LRU segment.
    /// @param size The maximum number of items in the protected segment.
    void set_protected_segment_size(const size_t size)
    {
        m_protected_segment_size = size;
    }

    /// @brief Insertion event handler.
    /// @details Inserts the provided item at the front of the probation segment.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in the cache.
    void on_insert(const Key& key, const CacheItem& /* item */)
    {
        assert(m_probation_nodes.find(key) == m_probation_nodes.end());

        m_probation_list.emplace_front(std::ref(key));
        m_probation_nodes.emplace(std::ref(key), m_probation_list.begin());
    }

    /// @brief Update event handler.
    /// @details If the item is in the probation segment, it is moved to the protected
    ///          segment. If the item is in the protected segment, it is moved to the front of
    ///          the protected segment.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key
    void on_update(const Key& key, const CacheItem& /* old_item */, const CacheItem& new_item)
    {
        on_cache_hit(key, new_item);
    }

    /// @brief Cache hit event handler.
    /// @details If the item is in the probation segment, it is moved to the protected
    ///          segment. If the item is in the protected segment, it is moved to the front of
    ///          the protected segment.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& /* item */)
    {
        assert(m_probation_nodes.size() == m_probation_list.size());
        assert(m_protected_nodes.size() == m_protected_list.size());

        auto protected_node_it = m_protected_nodes.find(key);
        if (protected_node_it != m_protected_nodes.end()) {
            if (protected_node_it->second != m_protected_list.begin()) {
                // If the node is in the protected segment, move it to the front of the protected segment.
                m_protected_list.splice(m_protected_list.begin(), m_protected_list, protected_node_it->second);
            }
        } else {
            // If the node is in probation, move it to the protected segment.
            [[maybe_unused]] const bool promotion_ok = move_to_protected(key);
            assert(promotion_ok);
        }

        trim_protected_segment();

        assert(m_probation_nodes.size() == m_probation_list.size());
        assert(m_protected_nodes.size() == m_protected_list.size());
    }

    /// @brief Eviction event handler.
    /// @details Removes the item from the segment it belongs to.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& key, const CacheItem& /* item */)
    {
        assert((!m_protected_list.empty()) || !m_probation_list.empty());

        auto key_and_it = m_probation_nodes.find(key);
        if (key_and_it != m_probation_nodes.end()) {
            m_probation_list.erase(key_and_it->second);
            m_probation_nodes.erase(key_and_it);
        } else {
            key_and_it = m_protected_nodes.find(key);
            assert(key_and_it != m_protected_nodes.end());
            m_protected_list.erase(key_and_it->second);
            m_protected_nodes.erase(key_and_it);
        }
    }

    /// @brief Remove and return the next victim according to the S-LRU ordering.
    /// @details Victims are taken from the probation segment first, then from the protected segment when probation is empty.
    /// @return A ticket that can later be committed by the caller or restored with `rollback()`.
    [[nodiscard]] Ticket pop_victim()
    {
        if (!m_probation_list.empty()) {
            return pop_victim_from_probation();
        }
        return pop_victim_from_protected();
    }

    /// @brief Restore a victim previously returned by `pop_victim()`.
    /// @param ticket The ticket describing the victim to restore.
    void rollback(Ticket ticket)
    {
        if (ticket.m_from_probation) {
            assert(m_probation_nodes.find(ticket.m_key) == m_probation_nodes.end());
            m_probation_list.emplace_back(ticket.m_key);
            m_probation_nodes.emplace(ticket.m_key, std::prev(m_probation_list.end()));
        } else {
            assert(m_protected_nodes.find(ticket.m_key) == m_protected_nodes.end());
            m_protected_list.emplace_back(ticket.m_key);
            m_protected_nodes.emplace(ticket.m_key, std::prev(m_protected_list.end()));
        }
    }

private:
    size_t m_protected_segment_size;

    std::list<KeyRef> m_probation_list;
    KeyRefMap         m_probation_nodes;

    std::list<KeyRef> m_protected_list;
    KeyRefMap         m_protected_nodes;

    bool move_to_protected(const Key& key)
    {
        auto probation_node_it = m_probation_nodes.find(key);
        if (probation_node_it == m_probation_nodes.end()) {
            return false;
        }

        m_protected_list.splice(m_protected_list.begin(), m_probation_list, probation_node_it->second);
        m_probation_nodes.erase(key);
        m_protected_nodes.emplace(key, m_protected_list.begin());
        return true;
    }

    bool pop_to_probation()
    {
        if (m_protected_list.empty()) {
            return false;
        }

        m_probation_list.splice(m_probation_list.begin(), m_protected_list, --m_protected_list.end());
        m_protected_nodes.erase(*m_probation_list.begin());
        m_probation_nodes.emplace(*m_probation_list.begin(), m_probation_list.begin());
        return true;
    }

    void trim_protected_segment()
    {
        while (m_protected_list.size() > m_protected_segment_size) {
            [[maybe_unused]] const bool demotion_ok = pop_to_probation();
            assert(demotion_ok);
            assert(m_protected_list.size() == m_protected_segment_size);
        }
    }

    Ticket pop_victim_from_probation()
    {
        assert(!m_probation_list.empty());
        const KeyRef victim_key = m_probation_list.back();
        m_probation_nodes.erase(victim_key);
        m_probation_list.pop_back();
        return Ticket{victim_key, true};
    }

    Ticket pop_victim_from_protected()
    {
        assert(!m_protected_list.empty());
        const KeyRef victim_key = m_protected_list.back();
        m_protected_nodes.erase(victim_key);
        m_protected_list.pop_back();
        return Ticket{victim_key, false};
    }
};

}  // namespace cachemere::policy
