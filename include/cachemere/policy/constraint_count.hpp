namespace cachemere::policy {

template<typename K, typename V> ConstraintCount<K, V>::ConstraintCount(size_t maximum_count)
{
    update(maximum_count);
}

template<typename K, typename V> void ConstraintCount<K, V>::clear()
{
    m_count = 0;
}

template<typename K, typename V> bool ConstraintCount<K, V>::can_add(const K& /* key */, const CacheItem& /* item */)
{
    return m_count < m_maximum_count;
}

template<typename K, typename V> bool ConstraintCount<K, V>::can_replace(const K& /* key */, const CacheItem& /* old_item */, const CacheItem& /* new_item */)
{
    assert(m_count > 0);

    // Replacement doesn't change the count, so it's always allowed.
    return true;
}

template<typename K, typename V> bool ConstraintCount<K, V>::is_invalidated(const K& /* key */)
{
    return false;
}

template<typename K, typename V> bool ConstraintCount<K, V>::is_satisfied()
{
    return m_count <= m_maximum_count;
}

template<typename K, typename V> void ConstraintCount<K, V>::update(size_t maximum_count)
{
    m_maximum_count = maximum_count;
}

template<typename K, typename V> void ConstraintCount<K, V>::on_insert(const K& /* key */, const CacheItem& /* item */)
{
    ++m_count;
}

template<typename K, typename V> void ConstraintCount<K, V>::on_evict(const K& /* key */, const CacheItem& /* item */)
{
    assert(m_count > 0);
    --m_count;
}

template<typename K, typename V> size_t ConstraintCount<K, V>::count() const
{
    return m_count;
}

template<typename K, typename V> size_t ConstraintCount<K, V>::maximum_count() const
{
    return m_maximum_count;
}

}  // namespace cachemere::policy
