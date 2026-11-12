namespace cachemere::policy {

template<class K, class KH, class V, class Clock>
BasicConstraintTimeToLive<K, KH, V, Clock>::BasicConstraintTimeToLive(Duration time_to_live) : m_time_to_live{time_to_live}
{
}

template<class K, class KH, class V, class Clock>
BasicConstraintTimeToLive<K, KH, V, Clock>::BasicConstraintTimeToLive(const BasicConstraintTimeToLive& other)
 : m_time_to_live{other.m_time_to_live},
   m_use_order{other.m_use_order}
{
    // The other policy's nodes point in its own use order, so they're rebuilt to point in the copied one.
    m_nodes.reserve(m_use_order.size());
    for (auto it = m_use_order.begin(); it != m_use_order.end(); ++it) {
        m_nodes.emplace(it->first, it);
    }
}

template<class K, class KH, class V, class Clock>
auto BasicConstraintTimeToLive<K, KH, V, Clock>::operator=(const BasicConstraintTimeToLive& other) -> BasicConstraintTimeToLive&
{
    if (this != &other) {
        *this = BasicConstraintTimeToLive{other};
    }
    return *this;
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::clear()
{
    m_use_order.clear();
    m_nodes.clear();
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
    const auto key_and_it = m_nodes.find(key);
    assert(key_and_it != m_nodes.end());
    return has_expired(key_and_it->second->second);
}

template<class K, class KH, class V, class Clock> const K* BasicConstraintTimeToLive<K, KH, V, Clock>::next_expired() const
{
    // The least recently used item expires first.
    if (m_use_order.empty() || !has_expired(m_use_order.back().second)) {
        return nullptr;
    }
    return &m_use_order.back().first.get();
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::on_insert(const K& key, const CacheItem& /* item */)
{
    assert(m_nodes.find(key) == m_nodes.end());
    m_use_order.emplace_front(key, Clock::now());
    m_nodes.emplace(m_use_order.front().first, m_use_order.begin());
}

template<class K, class KH, class V, class Clock>
void BasicConstraintTimeToLive<K, KH, V, Clock>::on_update(const K& key, const CacheItem& /* old_item */, const CacheItem& /* new_item */)
{
    mark_used(key);
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::on_cache_hit(const K& key, const CacheItem& /* item */)
{
    mark_used(key);
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::on_evict(const K& key, const CacheItem& /* item */)
{
    const auto key_and_it = m_nodes.find(key);
    assert(key_and_it != m_nodes.end());
    m_use_order.erase(key_and_it->second);
    m_nodes.erase(key_and_it);
}

template<class K, class KH, class V, class Clock> bool BasicConstraintTimeToLive<K, KH, V, Clock>::has_expired(TimePoint last_used) const
{
    // Compare the elapsed time: adding the time-to-live to the time point overflows when the time-to-live is long.
    return Clock::now() - last_used > m_time_to_live;
}

template<class K, class KH, class V, class Clock> void BasicConstraintTimeToLive<K, KH, V, Clock>::mark_used(const K& key)
{
    const auto key_and_it = m_nodes.find(key);
    assert(key_and_it != m_nodes.end());
    key_and_it->second->second = Clock::now();
    m_use_order.splice(m_use_order.begin(), m_use_order, key_and_it->second);
}

}  // namespace cachemere::policy
