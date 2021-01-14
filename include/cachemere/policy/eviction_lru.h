#ifndef CACHEMERE_EVICTION_LRU
#define CACHEMERE_EVICTION_LRU

#include <functional>
#include <list>
#include <map>

#include "cachemere/detail/item.h"

namespace cachemere::policy {

template<typename Key, typename Value> class EvictionLRU
{
private:

    struct KeyInfo {
        KeyInfo(const Key& key, size_t overhead);
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

        VictimIterator(const KeyInfoReverseIt& p_Iterator);

        const Key&      operator*() const;
        VictimIterator& operator++();
        VictimIterator  operator++(int);
        bool            operator==(const VictimIterator& other) const;
        bool            operator!=(const VictimIterator& other) const;

    private:
        KeyInfoReverseIt m_iterator;
    };

    void on_insert(const CacheItem& item);
    void on_update(const CacheItem& item);
    void on_cache_hit(const CacheItem& item);
    void on_evict(const Key& item);

    [[nodiscard]] VictimIterator victim_begin() const;
    [[nodiscard]] VictimIterator victim_end() const;

    [[nodiscard]] size_t memory_used() const noexcept;

    constexpr static size_t item_overhead_size = sizeof(KeyInfoIt) + sizeof(KeyRef);

private:
    std::list<KeyInfo> m_keys;
    KeyRefMap          m_nodes;
    size_t             m_size = 0;
};

}  // namespace cachemere::policy

#include "eviction_lru.hpp"

#endif
