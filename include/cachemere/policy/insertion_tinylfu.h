#ifndef CACHEMERE_INSERTION_TINYLFU_H
#define CACHEMERE_INSERTION_TINYLFU_H

#include <cstdint>

#include "cachemere/detail/item.h"

#include "detail/bloom_filter.h"
#include "detail/counting_bloom_filter.h"

namespace cachemere::policy {

template<typename Key, typename Value> class InsertionTinyLFU
{
public:
    using CacheItem = cachemere::detail::Item<Key, Value>;

    // Event handlers.
    void on_cache_hit(const CacheItem& item);
    void on_cache_miss(const Key& key);

    // Policy interface.
    void reserve(uint32_t cardinality);
    bool should_add(const Key& key);
    bool should_replace(const Key& victim, const Key& candidate);

    // Statistics
    [[nodiscard]] size_t memory_used() const noexcept;

private:
    const static uint32_t            DEFAULT_CACHE_CARDINALITY = 2000;
    detail::BloomFilter<Key>         m_gatekeeper{DEFAULT_CACHE_CARDINALITY};
    detail::CountingBloomFilter<Key> m_frequency_sketch{DEFAULT_CACHE_CARDINALITY};

    uint32_t estimate_count_for_key(const Key& key) const;
    void     reset();
    void     touch_item(const Key& key);
};

}  // namespace cachemere::policy

#include "insertion_tinylfu.hpp"

#endif
