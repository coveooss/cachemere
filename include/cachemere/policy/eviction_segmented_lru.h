#ifndef CACHEMERE_EVICTION_SEGMENTED_LRU_H
#define CACHEMERE_EVICTION_SEGMENTED_LRU_H

#include <list>

#include "cachemere/detail/item.h"

namespace cachemere::policy {

template<typename Key, typename Value> class EvictionSegmentedLRU
{
private:
    struct KeyInfo {
        KeyInfo(const Key& key, size_t key_size);
        KeyInfo(const KeyInfo&) = delete;
        KeyInfo& operator=(const KeyInfo&) = delete;

        const Key& m_key;
        size_t     m_overhead;
    };

    using KeyInfoIt = typename std::list<KeyInfo>::iterator;
    using KeyRef    = std::reference_wrapper<const Key>;
    using KeyRefMap = std::map<KeyRef, KeyInfoIt, std::less<>>;

public:
    using CacheItem = cachemere::detail::Item<Key, Value>;

    class VictimIterator
    {
    public:
        using KeyInfoReverseIt = typename std::list<KeyInfo>::const_reverse_iterator;

        VictimIterator(const KeyInfoReverseIt& probation_iterator, const KeyInfoReverseIt& probation_end_iterator, const KeyInfoReverseIt& protected_iterator);

        const Key&      operator*() const;
        VictimIterator& operator++();
        VictimIterator  operator++(int);
        bool            operator==(const VictimIterator& other) const;
        bool            operator!=(const VictimIterator& other) const;

    private:
        KeyInfoReverseIt m_probation_iterator;
        KeyInfoReverseIt m_probation_end_iterator;
        KeyInfoReverseIt m_protected_iterator;
        bool             m_done_with_probation;
    };

    void set_protected_segment_size(size_t size);

    // Event handlers.
    void on_insert(const CacheItem& item);
    void on_update(const CacheItem& item);
    void on_cache_hit(const CacheItem& item);
    void on_evict(const Key& item);

    // Policy interface.
    [[nodiscard]] VictimIterator victim_begin() const;
    [[nodiscard]] VictimIterator victim_end() const;

    [[nodiscard]] size_t memory_used() const noexcept;

private:
    size_t m_protected_segment_size;

    std::list<KeyInfo> m_probation_list;
    KeyRefMap          m_probation_nodes;

    std::list<KeyInfo> m_protected_list;
    KeyRefMap          m_protected_nodes;

    size_t m_size;

    bool move_to_protected(const Key& key);
    bool pop_to_probation();
};

}  // namespace cachemere::policy

#include "eviction_segmented_lru.hpp"

#endif
