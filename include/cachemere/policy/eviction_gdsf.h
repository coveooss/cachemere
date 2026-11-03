#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>

#include <absl/container/btree_map.h>

#include <cachemere/item.h>
#include <cachemere/detail/traits.h>

#include "detail/counting_bloom_filter.h"

namespace cachemere::policy {

template<typename T, typename Key, typename Value>
concept CostFn = requires(T t, Key key, Item<Value> item) {
    { t(key, item) } -> std::convertible_to<uint64_t>;
};

/// @brief Greedy-Dual-Size-Frequency (GDSF) eviction policy.
/// @details Generally, GDSF tries to first evict the items that will be the least costly
///          to reload, while taking into account access frequency.
///          GDSF is implemented using a priority queue sorted by a coefficient computed for each item.
///          Items are then evicted starting with the item with the smallest coefficient.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
/// @tparam Cost A functor taking a a `Key&` and a `const Item<Value>&` returning the cost to load this item in cache.
//          The cost must not exceed 2^64 and should ideally be quite a bit below this limit since the cost of the item is added to the
//          policy's internal 64-bit clock on every insertion.
template<cachemere::detail::traits::Key Key, cachemere::detail::traits::HasherFor<Key> KeyHash, typename Value, CostFn<Key, Value> Cost> class EvictionGDSF
{
private:
    using KeyRef = std::reference_wrapper<const Key>;

    struct PriorityEntry {
        PriorityEntry(KeyRef key, double coefficient) : m_key{key}, m_h_coefficient{coefficient}
        {
        }

        bool operator<(const PriorityEntry& other) const
        {
            return m_h_coefficient < other.m_h_coefficient;
        }

        KeyRef m_key;
        double m_h_coefficient;
    };

    using PrioritySet   = std::multiset<PriorityEntry>;
    using PrioritySetIt = typename PrioritySet::const_iterator;

public:
    /// @brief Eviction event struct, containing the key and the coefficient of the evicted item.
    /// @details This struct is used to rollback the policy state in case of an aborted insert.
    struct Ticket {
        Ticket(PriorityEntry entry, uint64_t previous_clock) : m_entry{std::move(entry)}, m_previous_clock{previous_clock}
        {
        }

        const Key& key() const
        {
            return m_entry.m_key;
        }

        PriorityEntry m_entry;
        uint64_t      m_previous_clock;
    };

    using CacheItem = Item<Value>;

    /// @brief Clears the policy.
    void clear()
    {
        m_priority_set.clear();
        m_iterator_map.clear();
        m_frequency_sketch.clear();
    }

    /// @brief Set the cardinality of the policy.
    /// @details The set cardinality should be a decent approximation of the cardinality
    ///          of the set of keys that _might_ be inserted in the cache. Getting an
    ///          accurate estimate is very important, because an underestimation will severely
    ///          decrease the accuracy of the policy, while an overestimation will use too much memory.
    /// @param cardinality The expected cardinality of the set of items.
    void set_cardinality(uint32_t cardinality)
    {
        m_frequency_sketch = detail::CountingBloomFilter<KeyHash>{cardinality};
    }

    /// @brief Insertion event handler.
    /// @details Computes a coefficient for the item and inserts it in the priority queue.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& key, const CacheItem& item)
    {
        m_frequency_sketch.add(key);

        assert(m_iterator_map.find(std::ref(key)) == m_iterator_map.end());

        const auto it = m_priority_set.emplace(std::ref(key), get_h_coefficient(key, item));
        m_iterator_map.emplace(std::ref(key), it);
    }

    /// @brief Update event handler.
    /// @details Updates the coefficient for this item and changes its position in the priority queue.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key.
    void on_update(const Key& key, const CacheItem& /* old_item */, const CacheItem& new_item)
    {
        on_cache_hit(key, new_item);
    }

    /// @brief Cache hit event handler.
    /// @details Updates the coefficient for this item and changes its position in the priority queue.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& item)
    {
        auto keyref_and_it = m_iterator_map.find(std::ref(key));
        assert(keyref_and_it != m_iterator_map.end());

        m_frequency_sketch.add(key);
        m_priority_set.erase(keyref_and_it->second);
        keyref_and_it->second = m_priority_set.emplace(std::ref(key), get_h_coefficient(key, item));
    }

    /// @brief Eviction event handler.
    /// @details Removes the item from the priority queue.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& key, const CacheItem& /* item */)
    {
        auto keyref_and_it = m_iterator_map.find(std::ref(key));
        assert(keyref_and_it != m_iterator_map.end());

        const auto priority_it = keyref_and_it->second;
        update_clock(static_cast<uint64_t>(priority_it->m_h_coefficient));

        m_priority_set.erase(priority_it);
        m_iterator_map.erase(keyref_and_it);
    }

    Ticket pop_victim()
    {
        assert(!m_priority_set.empty());

        auto          victim_it    = m_priority_set.begin();
        PriorityEntry victim_entry = *victim_it;
        m_priority_set.erase(victim_it);
        m_iterator_map.erase(victim_entry.m_key);

        const uint64_t previous_clock = m_clock;
        update_clock(static_cast<uint64_t>(victim_entry.m_h_coefficient));

        return Ticket{std::move(victim_entry), previous_clock};
    }

    void rollback(Ticket ticket)
    {
        const Key& victim_key = ticket.key();
        assert(m_iterator_map.find(std::ref(victim_key)) == m_iterator_map.end());

        const auto it = m_priority_set.emplace(std::move(ticket.m_entry));
        m_iterator_map.emplace(std::ref(victim_key), it);

        assert(m_clock >= ticket.m_previous_clock);
        m_clock = ticket.m_previous_clock;
    }

private:
    using IteratorMap = absl::btree_map<KeyRef, PrioritySetIt, std::less<const Key>>;

    const static uint32_t                DEFAULT_CACHE_CARDINALITY = 2000;               // The expected cache cardinality, for the counting bloom filter.
    mutable Cost                         m_measure_cost;                                 // The functor to measure the cost metric of cached items.
    detail::CountingBloomFilter<KeyHash> m_frequency_sketch{DEFAULT_CACHE_CARDINALITY};  // TODO: Replace with a count-min sketch to get rid of cardinality

    PrioritySet m_priority_set;  // A multiset of keys sorted by H-coefficients in ascending order.
    IteratorMap m_iterator_map;  // A map of keys pointing to the corresponding iterator in the priority set.

    uint64_t m_clock{0};

    [[nodiscard]] double get_h_coefficient(const Key& key, const CacheItem& item) const noexcept
    {
        return static_cast<double>(m_clock) + static_cast<double>(m_frequency_sketch.estimate(key)) *
                                                  (static_cast<double>(m_measure_cost(key, item)) / static_cast<double>(item.m_total_size));
    }

    void update_clock(uint64_t new_clock)
    {
        m_clock = std::max(m_clock, new_clock);
    }
};

}  // namespace cachemere::policy
