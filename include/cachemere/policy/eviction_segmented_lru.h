#ifndef CACHEMERE_EVICTION_SEGMENTED_LRU_H
#define CACHEMERE_EVICTION_SEGMENTED_LRU_H

#include <list>

#include <absl/container/flat_hash_map.h>

#include "cachemere/item.h"

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
template<typename Key, typename KeyHash, typename Value> class EvictionSegmentedLRU
{
private:
    using KeyRef    = std::reference_wrapper<const Key>;
    using KeyRefIt  = typename std::list<KeyRef>::iterator;
    using KeyRefMap = absl::flat_hash_map<KeyRef, KeyRefIt, KeyHash, std::equal_to<Key>>;

public:
    using CacheItem = cachemere::Item<Value>;

    /// @brief Iterator for iterating over cache items in the order they should be
    //         evicted.
    class VictimIterator
    {
    public:
        using KeyRefReverseIt = typename std::list<KeyRef>::const_reverse_iterator;

        VictimIterator(const KeyRefReverseIt& probation_iterator, const KeyRefReverseIt& probation_end_iterator, const KeyRefReverseIt& protected_iterator);

        const Key&      operator*() const;
        VictimIterator& operator++();
        VictimIterator  operator++(int);
        bool            operator==(const VictimIterator& other) const;
        bool            operator!=(const VictimIterator& other) const;

    private:
        KeyRefReverseIt m_probation_iterator;
        KeyRefReverseIt m_probation_end_iterator;
        KeyRefReverseIt m_protected_iterator;
        bool            m_done_with_probation;
    };

    /// @brief Clears the policy.
    void clear();

    /// @brief Set the maximum number of items in the protected LRU segment.
    /// @param size The maximum number of items in the protected segment.
    void set_protected_segment_size(size_t size);

    /// @brief Insertion event handler.
    /// @details Inserts the provided item at the front of the probation segment.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in the cache.
    void on_insert(const Key& key, const CacheItem& item);

    /// @brief Update event handler.
    /// @details If the item is in the probation segment, it is moved to the protected
    ///          segment. If the item is in the protected segment, it is moved to the front of
    ///          the protected segment.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key
    void on_update(const Key& key, const CacheItem& old_item, const CacheItem& new_item);

    /// @brief Cache hit event handler.
    /// @details If the item is in the probation segment, it is moved to the protected
    ///          segment. If the item is in the protected segment, it is moved to the front of
    ///          the protected segment.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& item);

    /// @brief Eviction event handler.
    /// @details Removes the item from the segment it belongs to.
    /// @param key The key that was evicted.
    /// @param item The item that was evicted.
    void on_evict(const Key& key, const CacheItem& item);

    /// @brief Get an iterator to the first item that should be evicted.
    /// @details Considering that the keys are ordered internally from most-recently used
    ///          to least-recently used, this iterator will effectively walk the internal
    ///          structure backwards.
    /// @return An item iterator.
    [[nodiscard]] VictimIterator victim_begin() const;

    /// @brief Get an end iterator.
    /// @return The end iterator.
    [[nodiscard]] VictimIterator victim_end() const;

private:
    size_t m_protected_segment_size;

    std::list<KeyRef> m_probation_list;
    KeyRefMap         m_probation_nodes;

    std::list<KeyRef> m_protected_list;
    KeyRefMap         m_protected_nodes;

    bool move_to_protected(const Key& key);
    bool pop_to_probation();
};

}  // namespace cachemere::policy

#include "eviction_segmented_lru.hpp"

#endif
