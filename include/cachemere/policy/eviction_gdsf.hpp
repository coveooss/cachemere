namespace cachemere::policy {

template<class Key, class Value, class Cost>
EvictionGDSF<Key, Value, Cost>::VictimIterator::VictimIterator(PrioritySetIt iterator) : m_iterator(std::move(iterator))
{
}

template<class Key, class Value, class Cost> const Key& EvictionGDSF<Key, Value, Cost>::VictimIterator::operator*() const
{
    return m_iterator->m_key;
}

template<class Key, class Value, class Cost> auto EvictionGDSF<Key, Value, Cost>::VictimIterator::operator++() -> VictimIterator&
{
    ++m_iterator;
    return *this;
}

template<class Key, class Value, class Cost> auto EvictionGDSF<Key, Value, Cost>::VictimIterator::operator++(int) -> VictimIterator
{
    return (*this)++;
}

template<class Key, class Value, class Cost> bool EvictionGDSF<Key, Value, Cost>::VictimIterator::operator==(const VictimIterator& other) const
{
    return m_iterator == other.m_iterator;
}

template<class Key, class Value, class Cost> bool EvictionGDSF<Key, Value, Cost>::VictimIterator::operator!=(const VictimIterator& other) const
{
    return m_iterator != other.m_iterator;
}

template<class Key, class Value, class Cost> void EvictionGDSF<Key, Value, Cost>::set_cardinality(uint32_t cardinality)
{
    m_frequency_sketch = detail::CountingBloomFilter<Key>{cardinality};
}

template<class Key, class Value, class Cost> void EvictionGDSF<Key, Value, Cost>::on_insert(const CacheItem& item)
{
    m_frequency_sketch.add(item.m_key);

    PrioritySetIt it                     = m_priority_set.emplace(std::ref(item.m_key), get_h_coefficient(item));
    m_iterator_map[std::ref(item.m_key)] = std::move(it);
}

template<class Key, class Value, class Cost> void EvictionGDSF<Key, Value, Cost>::on_update(const CacheItem& item)
{
    on_cache_hit(item);
}

template<class Key, class Value, class Cost> void EvictionGDSF<Key, Value, Cost>::on_cache_hit(const CacheItem& item)
{
    auto keyref_and_it = m_iterator_map.find(std::ref(item.m_key));
    assert(keyref_and_it != m_iterator_map.end());

    PrioritySetIt it = keyref_and_it->second;

    m_priority_set.erase(it);

    on_insert(item);
}

template<class Key, class Value, class Cost> void EvictionGDSF<Key, Value, Cost>::on_evict(const Key& key)
{
    auto keyref_and_it = m_iterator_map.find(std::ref(key));
    assert(keyref_and_it != m_iterator_map.end());

    PrioritySetIt it = keyref_and_it->second;
    m_clock          = std::max(static_cast<double>(m_clock), it->m_h_coefficient);

    m_priority_set.erase(it);
    m_iterator_map.erase(keyref_and_it);

    assert(m_iterator_map.find(key) == m_iterator_map.end());
}

template<class Key, class Value, class Cost> auto EvictionGDSF<Key, Value, Cost>::victim_begin() const -> VictimIterator
{
    return VictimIterator{std::move(m_priority_set.begin())};
}

template<class Key, class Value, class Cost> auto EvictionGDSF<Key, Value, Cost>::victim_end() const -> VictimIterator
{
    return VictimIterator{std::move(m_priority_set.end())};
}

template<class Key, class Value, class Cost> double EvictionGDSF<Key, Value, Cost>::get_h_coefficient(const CacheItem& item) const noexcept
{
    return static_cast<double>(m_clock) +
           static_cast<double>(m_frequency_sketch.estimate(item.m_key)) * (static_cast<double>(m_measure_cost(item)) / static_cast<double>(item.m_total_size));
}

}  // namespace cachemere::policy
