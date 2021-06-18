#ifndef CACHEMERE_EVICTION_GDSF
#define CACHEMERE_EVICTION_GDSF

#include <algorithm>
#include <map>
#include <set>

#include "cachemere/item.h"
#include "cachemere/policy/detail/counting_bloom_filter.h"

namespace cachemere::policy {

/// @brief Greedy-Dual-Size-Frequency (GDSF) eviction policy.
/// @details Generally, GDSF tries to first evict the items that will be the least costly
///          to reload, while taking into account access frequency.
///          GDSF is implemented using a priority queue sorted by a coefficient computed for each item.
///          Items are then evicted starting with the item with the smallest coefficient.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam Value The type of the values stored in the cache.
/// @tparam Cost A functor taking a a `Key&` and a `const Item<Value>&` returning the cost to load this item in cache.
template<typename Key, typename Value, typename Cost> class EvictionGDSF
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
    using CacheItem = cachemere::Item<Value>;

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
    /// @param item The item that has been updated in the cache.
    void on_update(const Key& key, const CacheItem& item);

    /// @brief Cache hit event handler.
    /// @details Updates the coefficient for this item and changes its position in the priority queue.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& item);

    /// @brief Eviction event handler.
    /// @details Removes the item from the priority queue.
    /// @param item The key of the item that was evicted.
    void on_evict(const Key& key);

    /// @brief Get an iterator to the first item that should be evicted.
    /// @return An item iterator.
    [[nodiscard]] VictimIterator victim_begin() const;

    /// @brief Get an end iterator.
    /// @return The end iterator.
    [[nodiscard]] VictimIterator victim_end() const;

private:
    using IteratorMap = std::map<KeyRef, PrioritySetIt, std::less<const Key>>;

    const static uint32_t            DEFAULT_CACHE_CARDINALITY = 2000;               // The expected cache cardinality, for the counting bloom filter.
    mutable Cost                     m_measure_cost;                                 // The functor to measure the cost metric of cached items.
    detail::CountingBloomFilter<Key> m_frequency_sketch{DEFAULT_CACHE_CARDINALITY};  // TODO: Replace with a count-min sketch to get rid of cardinality

    PrioritySet m_priority_set;  // A multiset of keys sorted by H-coefficients in ascending order.

    IteratorMap m_iterator_map;  // A map of keys pointing to the corresponding iterator in the priority set. This is necessary because since PrioritySet uses
                                 // coefficients to compare items to one another, we can't test for key membership directly.

    uint64_t m_clock{0};

    [[nodiscard]] double get_h_coefficient(const Key& key, const CacheItem& item) const noexcept;
};

}  // namespace cachemere::policy

#include "eviction_gdsf.hpp"

#endif
