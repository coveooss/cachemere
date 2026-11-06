namespace cachemere::policy {

template<class K, class KH, class V, class Clock>
BasicConstraintTimeToLive<K, KH, V, Clock>::BasicConstraintTimeToLive(Duration time_to_live) : m_time_to_live{time_to_live}
{
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::clear()
{
    m_last_used.clear();
}

template<class K, class KH, class V, class Clock> bool BasicConstraintTimeToLive<K, KH, V, Clock>::can_add(const K& /* key */, const CacheItem& /* item */)
{
    return true;
}

template<class K, class KH, class V, class Clock>
bool BasicConstraintTimeToLive<K, KH, V, Clock>::can_replace(const K& /* key */, const CacheItem& /* old_item */, const CacheItem& /* new_item */)
{
    return true;
}

template<class K, class KH, class V, class Clock> bool BasicConstraintTimeToLive<K, KH, V, Clock>::is_satisfied()
{
    return true;
}

template<class K, class KH, class V, class Clock> bool BasicConstraintTimeToLive<K, KH, V, Clock>::is_expired(const K& key) const
{
    const auto key_and_time = m_last_used.find(key);
    assert(key_and_time != m_last_used.end());
    return key_and_time->second + m_time_to_live < Clock::now();
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::on_insert(const K& key, const CacheItem& /* item */)
{
    m_last_used[key] = Clock::now();
}

template<class K, class KH, class V, class Clock>
void BasicConstraintTimeToLive<K, KH, V, Clock>::on_update(const K& key, const CacheItem& /* old_item */, const CacheItem& /* new_item */)
{
    m_last_used[key] = Clock::now();
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::on_cache_hit(const K& key, const CacheItem& /* item */)
{
    m_last_used[key] = Clock::now();
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::on_evict(const K& key, const CacheItem& /* item */)
{
    m_last_used.erase(key);
}

}  // namespace cachemere::policy
