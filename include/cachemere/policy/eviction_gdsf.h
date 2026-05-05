#pragma once

#include <algorithm>

#include <absl/container/btree_map.h>

#include <cachemere/item.h>

#include "detail/counting_bloom_filter.h"

namespace cachemere::policy {

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
template<typename Key, typename KeyHash, typename Value, typename Cost> class EvictionGDSF
{
private:
    using KeyRef = std::reference_wrapper<const Key>;

    struct PriorityEntry {
        PriorityEntry(KeyRef key, double coefficient);

        bool operator<(const PriorityEntry& other) const;

        KeyRef m_key;
        double m_h_coefficient;
    };

    using PrioritySet   = std::multiset<PriorityEntry>;
    using PrioritySetIt = typename PrioritySet::const_iterator;

public:
    using CacheItem = Item<Value>;

    /// @brief Iterator for iterating over cache items in the order they should be
    ///        evicted.
    class VictimIterator
    {
    public:
        VictimIterator(PrioritySetIt iterator);

        const Key&      operator*() const;
        VictimIterator& operator++();
        VictimIterator  operator++(int);
        bool            operator==(const VictimIterator& other) const;
        bool            operator!=(const VictimIterator& other) const;

    private:
        PrioritySetIt m_iterator;
    };

    /// @brief Clears the policy.
    void clear();

    /// @brief Set the cardinality of the policy.
    /// @details The set cardinality should be a decent approximation of the cardinality
    ///          of the set of keys that _might_ be inserted in the cache. Getting an
    ///          accurate estimate is very important, because an underestimation will severely
    ///          decrease the accuracy of the policy, while an overestimation will use too much memory.
    /// @param cardinality The expected cardinality of the set of items.
    void set_cardinality(uint32_t cardinality);

    /// @brief Insertion event handler.
    /// @details Computes a coefficient for the item and inserts it in the priority queue.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& key, const CacheItem& item);

    /// @brief Update event handler.
    /// @details Updates the coefficient for this item and changes its position in the priority queue.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key.
    void on_update(const Key& key, const CacheItem& old_item, const CacheItem& new_item);

    /// @brief Cache hit event handler.
    /// @details Updates the coefficient for this item and changes its position in the priority queue.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& item);

    /// @brief Eviction event handler.
    /// @details Removes the item from the priority queue.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& key, const CacheItem& item);

    /// @brief Get an iterator to the first item that should be evicted.
    /// @return An item iterator.
    [[nodiscard]] VictimIterator victim_begin() const;

    /// @brief Get an end iterator.
    /// @return The end iterator.
    [[nodiscard]] VictimIterator victim_end() const;

private:
    using IteratorMap = absl::btree_map<KeyRef, PrioritySetIt, std::less<const Key>>;

    const static uint32_t                DEFAULT_CACHE_CARDINALITY = 2000;               // The expected cache cardinality, for the counting bloom filter.
    mutable Cost                         m_measure_cost;                                 // The functor to measure the cost metric of cached items.
    detail::CountingBloomFilter<KeyHash> m_frequency_sketch{DEFAULT_CACHE_CARDINALITY};  // TODO: Replace with a count-min sketch to get rid of cardinality

    PrioritySet m_priority_set;  // A multiset of keys sorted by H-coefficients in ascending order.

    IteratorMap m_iterator_map;  // A map of keys pointing to the corresponding iterator in the priority set. This is necessary because since PrioritySet uses
                                 // coefficients to compare items to one another, we can't test for key membership directly.

    uint64_t m_clock{0};

    [[nodiscard]] double get_h_coefficient(const Key& key, const CacheItem& item) const noexcept;
};

template<class Key, class KeyHash, class Value, class Cost>
EvictionGDSF<Key, KeyHash, Value, Cost>::PriorityEntry::PriorityEntry(KeyRef key, double coefficient) : m_key(key),
                                                                                                        m_h_coefficient(coefficient)
{
}

template<class Key, class KeyHash, class Value, class Cost>
bool EvictionGDSF<Key, KeyHash, Value, Cost>::PriorityEntry::operator<(const PriorityEntry& other) const
{
    return m_h_coefficient < other.m_h_coefficient;
}

template<class Key, class KeyHash, class Value, class Cost>
EvictionGDSF<Key, KeyHash, Value, Cost>::VictimIterator::VictimIterator(PrioritySetIt iterator) : m_iterator(std::move(iterator))
{
}

template<class Key, class KeyHash, class Value, class Cost> const Key& EvictionGDSF<Key, KeyHash, Value, Cost>::VictimIterator::operator*() const
{
    return m_iterator->m_key;
}

template<class Key, class KeyHash, class Value, class Cost> auto EvictionGDSF<Key, KeyHash, Value, Cost>::VictimIterator::operator++() -> VictimIterator&
{
    ++m_iterator;
    return *this;
}

template<class Key, class KeyHash, class Value, class Cost> auto EvictionGDSF<Key, KeyHash, Value, Cost>::VictimIterator::operator++(int) -> VictimIterator
{
    return (*this)++;
}

template<class Key, class KeyHash, class Value, class Cost>
bool EvictionGDSF<Key, KeyHash, Value, Cost>::VictimIterator::operator==(const VictimIterator& other) const
{
    return m_iterator == other.m_iterator;
}

template<class Key, class KeyHash, class Value, class Cost>
bool EvictionGDSF<Key, KeyHash, Value, Cost>::VictimIterator::operator!=(const VictimIterator& other) const
{
    return m_iterator != other.m_iterator;
}

template<class Key, class KeyHash, class Value, class Cost> void EvictionGDSF<Key, KeyHash, Value, Cost>::clear()
{
    m_priority_set.clear();
    m_iterator_map.clear();
    m_frequency_sketch.clear();
}

template<class Key, class KeyHash, class Value, class Cost> void EvictionGDSF<Key, KeyHash, Value, Cost>::set_cardinality(uint32_t cardinality)
{
    m_frequency_sketch = detail::CountingBloomFilter<Key>{cardinality};
}

template<class Key, class KeyHash, class Value, class Cost> void EvictionGDSF<Key, KeyHash, Value, Cost>::on_insert(const Key& key, const CacheItem& item)
{
    m_frequency_sketch.add(key);

    PrioritySetIt it              = m_priority_set.emplace(std::ref(key), get_h_coefficient(key, item));
    m_iterator_map[std::ref(key)] = std::move(it);
}

template<class Key, class KeyHash, class Value, class Cost>
void EvictionGDSF<Key, KeyHash, Value, Cost>::on_update(const Key& key, const CacheItem& /* old_item */, const CacheItem& new_item)
{
    on_cache_hit(key, new_item);
}

template<class Key, class KeyHash, class Value, class Cost> void EvictionGDSF<Key, KeyHash, Value, Cost>::on_cache_hit(const Key& key, const CacheItem& item)
{
    auto keyref_and_it = m_iterator_map.find(std::ref(key));
    assert(keyref_and_it != m_iterator_map.end());

    PrioritySetIt it = keyref_and_it->second;

    m_priority_set.erase(it);

    on_insert(key, item);
}

template<class Key, class KeyHash, class Value, class Cost> void EvictionGDSF<Key, KeyHash, Value, Cost>::on_evict(const Key& key, const CacheItem& /* item */)
{
    auto keyref_and_it = m_iterator_map.find(std::ref(key));
    assert(keyref_and_it != m_iterator_map.end());

    PrioritySetIt it = keyref_and_it->second;
    m_clock          = std::max(m_clock, static_cast<uint64_t>(it->m_h_coefficient));

    m_priority_set.erase(it);
    m_iterator_map.erase(keyref_and_it);

    assert(m_iterator_map.find(key) == m_iterator_map.end());
}

template<class Key, class KeyHash, class Value, class Cost> auto EvictionGDSF<Key, KeyHash, Value, Cost>::victim_begin() const -> VictimIterator
{
    return VictimIterator{std::move(m_priority_set.begin())};
}

template<class Key, class KeyHash, class Value, class Cost> auto EvictionGDSF<Key, KeyHash, Value, Cost>::victim_end() const -> VictimIterator
{
    return VictimIterator{std::move(m_priority_set.end())};
}

template<class Key, class KeyHash, class Value, class Cost>
double EvictionGDSF<Key, KeyHash, Value, Cost>::get_h_coefficient(const Key& key, const CacheItem& item) const noexcept
{
    return static_cast<double>(m_clock) +
           static_cast<double>(m_frequency_sketch.estimate(key)) * (static_cast<double>(m_measure_cost(key, item)) / static_cast<double>(item.m_total_size));
}

}  // namespace cachemere::policy
