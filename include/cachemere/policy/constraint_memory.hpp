namespace cachemere::policy {

template<typename K, typename V> ConstraintMemory<K, V>::ConstraintMemory(size_t max_memory)
{
    update(max_memory);
}

template<typename K, typename V> void ConstraintMemory<K, V>::clear()
{
    m_memory = 0;
}

template<typename K, typename V> bool ConstraintMemory<K, V>::can_add(const K& /* key */, const CacheItem& item)
{
    return (m_memory + item.m_total_size) <= m_maximum_memory;
}

template<typename K, typename V> bool ConstraintMemory<K, V>::can_replace(const K& /* key */, const CacheItem& old_item, const CacheItem& new_item)
{
    assert(old_item.m_key_size == new_item.m_key_size);  // Key size *really* shouldn't have changed since the key is supposed to be const.
    return ((m_memory - old_item.m_value_size) + new_item.m_value_size) <= m_maximum_memory;
}

template<typename K, typename V> bool ConstraintMemory<K, V>::is_satisfied()
{
    return m_memory <= m_maximum_memory;
}

template<typename K, typename V> void ConstraintMemory<K, V>::update(size_t max_memory)
{
    m_maximum_memory = max_memory;
}

template<typename K, typename V> size_t ConstraintMemory<K, V>::memory() const
{
    return m_memory;
}

template<typename K, typename V> size_t ConstraintMemory<K, V>::maximum_memory() const
{
    return m_maximum_memory;
}

template<typename K, typename V> void ConstraintMemory<K, V>::on_insert(const K& /* key */, const CacheItem& item)
{
    m_memory += item.m_total_size;
    assert(m_memory <= m_maximum_memory);
}

template<typename K, typename V> void ConstraintMemory<K, V>::on_update(const K& /* key */, const CacheItem& old_item, const CacheItem& new_item)
{
    m_memory -= old_item.m_value_size;
    m_memory += new_item.m_value_size;
    assert(m_memory <= m_maximum_memory);
}

template<typename K, typename V> void ConstraintMemory<K, V>::on_evict(const K& /* key */, const CacheItem& item)
{
    assert(item.m_total_size <= m_memory);
    m_memory -= item.m_total_size;
}

}  // namespace cachemere::policy
