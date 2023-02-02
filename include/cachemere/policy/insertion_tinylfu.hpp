namespace cachemere::policy {

template<class Key, class KeyHash, class Value> void InsertionTinyLFU<Key, KeyHash, Value>::clear()
{
    m_gatekeeper.clear();
    m_frequency_sketch.clear();
}

template<class Key, class KeyHash, class Value> void InsertionTinyLFU<Key, KeyHash, Value>::on_cache_hit(const Key& key, const CacheItem& /* item */)
{
    touch_item(key);
}

template<class Key, class KeyHash, class Value> template<class KeyType> void InsertionTinyLFU<Key, KeyHash, Value>::on_cache_miss(const KeyType& key)
{
    touch_item(key);
}

template<class Key, class KeyHash, class Value> void InsertionTinyLFU<Key, KeyHash, Value>::set_cardinality(uint32_t cardinality)
{
    m_gatekeeper       = detail::BloomFilter<KeyHash>(cardinality);
    m_frequency_sketch = detail::CountingBloomFilter<KeyHash>(cardinality);
}

template<class Key, class KeyHash, class Value> bool InsertionTinyLFU<Key, KeyHash, Value>::should_add(const Key& key)
{
    return m_gatekeeper.maybe_contains(key);
}

template<class Key, class KeyHash, class Value> bool InsertionTinyLFU<Key, KeyHash, Value>::should_replace(const Key& victim, const Key& candidate)
{
    return estimate_count_for_key(candidate) > estimate_count_for_key(victim);
}

template<class Key, class KeyHash, class Value> uint32_t InsertionTinyLFU<Key, KeyHash, Value>::estimate_count_for_key(const Key& key) const
{
    uint32_t sketch_estimation = m_frequency_sketch.estimate(key);
    if (m_gatekeeper.maybe_contains(key)) {
        ++sketch_estimation;
    }

    return sketch_estimation;
}

template<class Key, class KeyHash, class Value> void InsertionTinyLFU<Key, KeyHash, Value>::reset()
{
    m_gatekeeper.clear();
    m_frequency_sketch.decay();
}

template<class Key, class KeyHash, class Value> template<class KeyType> void InsertionTinyLFU<Key, KeyHash, Value>::touch_item(const KeyType& key)
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
