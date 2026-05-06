#pragma once

#include <cassert>
#include <functional>
#include <list>

#include <absl/container/flat_hash_map.h>

#include <cachemere/item.h>

namespace cachemere::policy {

/// @brief Least Recently Used (LRU) eviction policy.
/// @details Implemented internally using a linked list.
///          The keys are ordered from most-recently used to least-recently used.
///          Only stores references to keys kept alive by the cache.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
template<typename Key, typename KeyHash, typename Value> class EvictionLRU
{
private:
    using KeyRef    = std::reference_wrapper<const Key>;
    using KeyRefIt  = typename std::list<KeyRef>::iterator;
    using KeyRefMap = absl::flat_hash_map<KeyRef, KeyRefIt, KeyHash, std::equal_to<Key>>;

public:
    using CacheItem = cachemere::Item<Value>;

    /// @brief Iterator for iterating over cache items in the order they should be
    ///        evicted.
    class VictimIterator
    {
    public:
        using KeyRefReverseIt = typename std::list<KeyRef>::const_reverse_iterator;

        VictimIterator(const KeyRefReverseIt& p_Iterator);

        const Key&      operator*() const;
        VictimIterator& operator++();
        VictimIterator  operator++(int);
        bool            operator==(const VictimIterator& other) const;
        bool            operator!=(const VictimIterator& other) const;

    private:
        KeyRefReverseIt m_iterator;
    };

    /// @brief Clears the policy.
    void clear();

    /// @brief Insertion event handler.
    /// @details Inserts the provided item at the front of the list.
    /// @param key The key of the inserted item.
    /// @param item The item that has been inserted in cache.
    void on_insert(const Key& key, const CacheItem& item);

    /// @brief Update event handler.
    /// @details Moves the provided item to the front of the list.
    /// @param key The key that has been updated in the cache.
    /// @param old_item The old value for this key.
    /// @param new_item The new value for this key
    void on_update(const Key& key, const CacheItem& old_item, const CacheItem& new_item);

    /// @brief Cache hit event handler.
    /// @details Moves the provided item at the front of the list.
    /// @param key The key that has been hit.
    /// @param item The item that has been hit.
    void on_cache_hit(const Key& key, const CacheItem& item);

    /// @brief Eviction event handler.
    /// @details Removes the item at the back of the list - ensuring it has the provided key.
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
    std::list<KeyRef> m_keys;
    KeyRefMap         m_nodes;
};

template<class Key, class KeyHash, class Value>
EvictionLRU<Key, KeyHash, Value>::VictimIterator::VictimIterator(const KeyRefReverseIt& iterator) : m_iterator(iterator)
{
}

template<class Key, class KeyHash, class Value> const Key& EvictionLRU<Key, KeyHash, Value>::VictimIterator::operator*() const
{
    return *m_iterator;
}

template<class Key, class KeyHash, class Value> auto EvictionLRU<Key, KeyHash, Value>::VictimIterator::operator++() -> VictimIterator&
{
    ++m_iterator;
    return *this;
}

template<class Key, class KeyHash, class Value> auto EvictionLRU<Key, KeyHash, Value>::VictimIterator::operator++(int) -> VictimIterator
{
    auto tmp = *this;
    ++m_iterator;
    return tmp;
}

template<class Key, class KeyHash, class Value> bool EvictionLRU<Key, KeyHash, Value>::VictimIterator::operator==(const VictimIterator& other) const
{
    return m_iterator == other.m_iterator;
}

template<class Key, class KeyHash, class Value> bool EvictionLRU<Key, KeyHash, Value>::VictimIterator::operator!=(const VictimIterator& other) const
{
    return m_iterator != other.m_iterator;
}

template<class Key, class KeyHash, class Value> void EvictionLRU<Key, KeyHash, Value>::clear()
{
    m_keys.clear();
    m_nodes.clear();
}

template<class Key, class KeyHash, class Value> void EvictionLRU<Key, KeyHash, Value>::on_insert(const Key& key, const CacheItem& /* item */)
{
    assert(m_nodes.find(std::ref(key)) == m_nodes.end());  // Validate the item is not already in policy.

    m_keys.emplace_front(std::ref(key));
    m_nodes.emplace(std::ref(key), m_keys.begin());
}

template<class Key, class KeyHash, class Value>
void EvictionLRU<Key, KeyHash, Value>::on_update(const Key& key, const CacheItem& /* old_item */, const CacheItem& new_item)
{
    on_cache_hit(key, new_item);
}

template<class Key, class KeyHash, class Value> void EvictionLRU<Key, KeyHash, Value>::on_cache_hit(const Key& key, const CacheItem& /* item */)
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

template<class Key, class KeyHash, class Value> void EvictionLRU<Key, KeyHash, Value>::on_evict(const Key& key, const CacheItem& /* item */)
{
    assert(!m_nodes.empty());
    assert(!m_keys.empty());

    if (m_keys.back().get() == key) {
        m_nodes.erase(key);
        m_keys.pop_back();
    } else {
        auto it = m_nodes.find(key);
        assert(it != m_nodes.end());
        m_nodes.erase(it);
    }
}

template<class Key, class KeyHash, class Value> auto EvictionLRU<Key, KeyHash, Value>::victim_begin() const -> VictimIterator
{
    return VictimIterator{m_keys.rbegin()};
}

template<class Key, class KeyHash, class Value> auto EvictionLRU<Key, KeyHash, Value>::victim_end() const -> VictimIterator
{
    return VictimIterator{m_keys.rend()};
}

}  // namespace cachemere::policy
