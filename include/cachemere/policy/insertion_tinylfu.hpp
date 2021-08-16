namespace cachemere::policy {

template<typename Key, typename Value> void InsertionTinyLFU<Key, Value>::clear()
{
    m_gatekeeper.clear();
    m_frequency_sketch.clear();
}

template<typename Key, typename Value> void InsertionTinyLFU<Key, Value>::on_cache_hit(const Key& key, const CacheItem& /* item */)
{
    touch_item(key);
}

template<typename Key, typename Value> void InsertionTinyLFU<Key, Value>::on_cache_miss(const Key& key)
{
    touch_item(key);
}

template<typename Key, typename Value> void InsertionTinyLFU<Key, Value>::set_cardinality(uint32_t cardinality)
{
    m_gatekeeper       = detail::BloomFilter<Key>(cardinality);
    m_frequency_sketch = detail::CountingBloomFilter<Key>(cardinality);
}

template<typename Key, typename Value> bool InsertionTinyLFU<Key, Value>::should_add(const Key& key)
{
    return m_gatekeeper.maybe_contains(key);
}

template<typename Key, typename Value> bool InsertionTinyLFU<Key, Value>::should_replace(const Key& victim, const Key& candidate)
{
    return estimate_count_for_key(candidate) > estimate_count_for_key(victim);
}

template<typename Key, typename Value> uint32_t InsertionTinyLFU<Key, Value>::estimate_count_for_key(const Key& key) const
{
    uint32_t sketch_estimation = m_frequency_sketch.estimate(key);
    if (m_gatekeeper.maybe_contains(key)) {
        ++sketch_estimation;
    }

    return sketch_estimation;
}

template<typename Key, typename Value> void InsertionTinyLFU<Key, Value>::reset()
{
    m_gatekeeper.clear();
    m_frequency_sketch.decay();
}

template<typename Key, typename Value> void InsertionTinyLFU<Key, Value>::touch_item(const Key& key)
{
    if (m_gatekeeper.maybe_contains(key)) {
        m_frequency_sketch.add(key);
        if (m_frequency_sketch.estimate(key) > m_frequency_sketch.cardinality()) {
            reset();
        }
    } else {
        m_gatekeeper.add(key);
    }
}

}  // namespace cachemere::policy
