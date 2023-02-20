namespace cachemere::policy {

template<class K, class KH, class V> ConstraintCount<K, KH, V>::ConstraintCount(size_t maximum_count)
{
    update(maximum_count);
}

template<class K, class KH, class V> void ConstraintCount<K, KH, V>::clear()
{
    m_count = 0;
}

template<class K, class KH, class V> bool ConstraintCount<K, KH, V>::can_add(const K& /* key */, const CacheItem& /* item */)
{
    return m_count < m_maximum_count;
}

template<class K, class KH, class V>
bool ConstraintCount<K, KH, V>::can_replace(const K& /* key */, const CacheItem& /* old_item */, const CacheItem& /* new_item */)
{
    assert(m_count > 0);

    // Replacement doesn't change the count, so it's always allowed.
    return true;
}

template<class K, class KH, class V> bool ConstraintCount<K, KH, V>::is_satisfied()
{
    return m_count <= m_maximum_count;
}

template<class K, class KH, class V> void ConstraintCount<K, KH, V>::update(size_t maximum_count)
{
    m_maximum_count = maximum_count;
}

template<class K, class KH, class V> void ConstraintCount<K, KH, V>::on_insert(const K& /* key */, const CacheItem& /* item */)
{
    ++m_count;
}

template<class K, class KH, class V> void ConstraintCount<K, KH, V>::on_evict(const K& /* key */, const CacheItem& /* item */)
{
    assert(m_count > 0);
    --m_count;
}

template<class K, class KH, class V> size_t ConstraintCount<K, KH, V>::count() const
{
    return m_count;
}

template<class K, class KH, class V> size_t ConstraintCount<K, KH, V>::maximum_count() const
{
    return m_maximum_count;
}

}  // namespace cachemere::policy
