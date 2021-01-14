namespace cachemere::policy {

template<class Key, class Value> EvictionLRU<Key, Value>::KeyInfo::KeyInfo(const Key& key, size_t overhead) : m_key{key}, m_overhead{overhead}
{
}

template<class Key, class Value> EvictionLRU<Key, Value>::VictimIterator::VictimIterator(const KeyInfoReverseIt& iterator) : m_iterator(iterator)
{
}

template<class Key, class Value> const Key& EvictionLRU<Key, Value>::VictimIterator::operator*() const
{
    return m_iterator->m_key;
}

template<class Key, class Value> class EvictionLRU<Key, Value>::VictimIterator& EvictionLRU<Key, Value>::VictimIterator::operator++()
{
    ++m_iterator;
    return *this;
}

template<class Key, class Value>
class EvictionLRU<Key, Value>::VictimIterator EvictionLRU<Key, Value>::VictimIterator::operator++(int)
{
    return (*this)++;
}

template<class Key, class Value>
bool EvictionLRU<Key, Value>::VictimIterator::operator==(const VictimIterator& other) const
{
    return m_iterator == other.m_iterator;
}

template<class Key, class Value> bool EvictionLRU<Key, Value>::VictimIterator::operator!=(const VictimIterator& other) const
{
    return m_iterator != other.m_iterator;
}

template<class Key, class Value> void EvictionLRU<Key, Value>::on_insert(const CacheItem& item)
{
    assert(m_nodes.find(std::ref(item.m_key)) == m_nodes.end());  // Validate the item is not already in policy.

    m_keys.emplace_front(item.m_key, item_overhead_size);
    m_nodes.emplace(std::ref(item.m_key), m_keys.begin());
    m_size += item_overhead_size;
}

template<class Key, class Value> void EvictionLRU<Key, Value>::on_update(const CacheItem& item)
{
    on_cache_hit(item);
}

template<class Key, class Value> void EvictionLRU<Key, Value>::on_cache_hit(const CacheItem& item)
{
    auto node_it = m_nodes.find(item.m_key);
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

template<class Key, class Value> void EvictionLRU<Key, Value>::on_evict(const Key& key)
{
    assert(!m_nodes.empty());
    assert(!m_keys.empty());
    assert(m_keys.back().m_key == key);

    m_size -= m_keys.back().m_overhead;
    m_nodes.erase(key);
    m_keys.pop_back();
}

template<class Key, class Value> auto EvictionLRU<Key, Value>::victim_begin() const -> VictimIterator
{
    return VictimIterator{m_keys.rbegin()};
}

template<class Key, class Value> EvictionLRU<Key, Value>::victim_end() const->VictimIterator
{
    return VictimIterator{m_keys.rend()};
}

template<class Key, class Value> size_t EvictionLRU<Key, Value>::EvictionLRU::memory_used() const noexcept
{
    return m_size;
}

}  // namespace cachemere::policy
