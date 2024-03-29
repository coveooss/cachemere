namespace cachemere::policy {

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
    return (*this)++;
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
