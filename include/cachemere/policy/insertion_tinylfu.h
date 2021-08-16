#ifndef CACHEMERE_INSERTION_TINYLFU_H
#define CACHEMERE_INSERTION_TINYLFU_H

#include <cstdint>

#include "cachemere/item.h"

#include "detail/bloom_filter.h"
#include "detail/counting_bloom_filter.h"

namespace cachemere::policy {

/// @brief Tiny Least Frequently Used (TinyLFU) insertion policy.
/// @details TinyLFU is a state-of-the-art insertion policy that helps determine whether a given item should be
///          inserted and/or kept in cache while using a constant amount of memory. The policy uses a combination
///          of frequency sketches to keep track of items that have yet to be inserted in the cache, and uses those
///          sketches to decide which items should me prioritized.
template<typename Key, typename Value> class InsertionTinyLFU
{
public:
    using CacheItem = cachemere::Item<Value>;

    /// @brief Clears the policy.
    void clear();

    /// @brief Cache hit event handler.
    /// @details Updates the internal frequency sketches for the given item.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& item);

    /// @brief Cache miss event handler.
    /// @details Updates the internal frequency sketches for the given key.
    /// @param key The key that was missed.
    void on_cache_miss(const Key& key);

    /// @brief Set the cardinality of the policy.
    /// @details The set cardinality should be a decent approximation of the cardinality
    ///          of the set of keys that _might_ be inserted in the cache. Getting an
    ///          accurate estimate is very important, because an underestimation will severely
    ///          decrease the accuracy of the policy, while an overestimation will use too much memory.
    /// @param cardinality The expected cardinality of the set of items.
    void set_cardinality(uint32_t cardinality);

    /// @brief Determines whether a given key should be inserted into the cache.
    /// @details TinyLFU always accepts insertions of items if there is room for them in the cache.
    /// @param key The key of the insertion candidate.
    bool should_add(const Key& key);

    /// @brief Determines whether a given victim should be replaced by a given candidate.
    /// @details TinyLFU will use its internal sketches to build a frequency
    ///          estimation for both keys. It will return `true` only if it estimates
    ///          that the candidate key is accessed more frequently than the victim key.
    /// @param victim The key of the victim the candidate will be compared to.
    /// @param candidate The replacement candidate.
    bool should_replace(const Key& victim, const Key& candidate);

private:
    const static uint32_t            DEFAULT_CACHE_CARDINALITY = 2000;
    detail::BloomFilter<Key>         m_gatekeeper{DEFAULT_CACHE_CARDINALITY};        // TODO: Investigate using cuckoo filter here instead.
    detail::CountingBloomFilter<Key> m_frequency_sketch{DEFAULT_CACHE_CARDINALITY};  // TODO: Replace with count-min sketch to get rid of cardinality param.

    uint32_t estimate_count_for_key(const Key& key) const;
    void     reset();
    void     touch_item(const Key& key);
};

}  // namespace cachemere::policy

#include "insertion_tinylfu.hpp"

#endif
