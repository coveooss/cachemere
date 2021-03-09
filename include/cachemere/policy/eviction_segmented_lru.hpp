namespace cachemere::policy {

template<class Key, class Value>
EvictionSegmentedLRU<Key, Value>::VictimIterator::VictimIterator(const KeyRefReverseIt& probation_iterator,
                                                                 const KeyRefReverseIt& probation_end_iterator,
                                                                 const KeyRefReverseIt& protected_iterator)
 : m_probation_iterator{probation_iterator},
   m_probation_end_iterator{probation_end_iterator},
   m_protected_iterator{protected_iterator},
   m_done_with_probation{false}
{
}

template<class Key, class Value> const Key& EvictionSegmentedLRU<Key, Value>::VictimIterator::operator*() const
{
    const auto it = m_done_with_probation ? m_protected_iterator : m_probation_iterator;
    return *it;
}

template<class Key, class Value> class EvictionSegmentedLRU<Key, Value>::VictimIterator& EvictionSegmentedLRU<Key, Value>::VictimIterator::operator++()
{
    if (m_done_with_probation) {
        ++m_protected_iterator;
    } else {
        ++m_probation_iterator;
        m_done_with_probation |= m_probation_iterator == m_probation_end_iterator;
    }
    return *this;
}

template<class Key, class Value>
class EvictionSegmentedLRU<Key, Value>::VictimIterator EvictionSegmentedLRU<Key, Value>::VictimIterator::operator++(int)
{
    auto tmp = *this;
    ++*this;
    return tmp;
}

template<class Key, class Value>
bool EvictionSegmentedLRU<Key, Value>::VictimIterator::operator==(const VictimIterator& other) const
{
    return m_protected_iterator == other.m_protected_iterator && m_probation_iterator == other.m_probation_iterator &&
           m_probation_end_iterator == other.m_probation_end_iterator && m_done_with_probation == other.m_done_with_probation;
}

template<class Key, class Value> bool EvictionSegmentedLRU<Key, Value>::VictimIterator::operator!=(const VictimIterator& other) const
{
    return !(*this == other);
}

template<class Key, class Value> void EvictionSegmentedLRU<Key, Value>::set_protected_segment_size(size_t size)
{
    m_protected_segment_size = size;
}

template<class Key, class Value> void EvictionSegmentedLRU<Key, Value>::on_insert(const CacheItem& item)
{
    assert(m_probation_nodes.find(item.m_key) == m_probation_nodes.end());

    m_probation_list.emplace_front(std::ref(item.m_key));
    m_probation_nodes.emplace(std::ref(item.m_key), m_probation_list.begin());
}

template<class Key, class Value> void EvictionSegmentedLRU<Key, Value>::on_update(const CacheItem& item)
{
    on_cache_hit(item);
}

template<class Key, class Value> void EvictionSegmentedLRU<Key, Value>::on_cache_hit(const CacheItem& item)
{
    assert(m_probation_nodes.size() == m_probation_list.size());
    assert(m_protected_nodes.size() == m_protected_list.size());

    auto protected_node_it = m_protected_nodes.find(item.m_key);
    if (protected_node_it != m_protected_nodes.end()) {
        if (protected_node_it->second != m_protected_list.begin()) {
            // If the node is in the protected segment, move it to the front of the protected segment.
            m_protected_list.splice(m_protected_list.begin(), m_protected_list, protected_node_it->second);
        }
    } else {
        // If the node is in probation, move it to the protected segment.
        const bool promotion_ok = move_to_protected(item.m_key);
        assert(promotion_ok);
    }

    while (m_protected_list.size() > m_protected_segment_size) {
        const bool demotion_ok = pop_to_probation();
        assert(demotion_ok);
        assert(m_protected_list.size() == m_protected_segment_size);
    }

    assert(m_probation_nodes.size() == m_probation_list.size());
    assert(m_protected_nodes.size() == m_protected_list.size());
}

template<class Key, class Value> void EvictionSegmentedLRU<Key, Value>::on_evict(const Key& key)
{
    assert((m_probation_list.empty() && !m_protected_list.empty() && m_protected_list.back().get() == key) ||
           (!m_probation_list.empty() && m_probation_list.back().get() == key));

    if (!m_probation_list.empty() && m_probation_list.back().get() == key) {
        // There are items in the probation list, these should be evicted first.
        assert(m_probation_nodes.find(key) != m_probation_nodes.end());
        m_probation_nodes.erase(key);
        m_probation_list.pop_back();
    } else {
        // The probation list is empty, evict from the protected list.
        assert(m_protected_nodes.find(key) != m_protected_nodes.end());
        m_protected_nodes.erase(key);
        m_protected_list.pop_back();
    }
}

template<class Key, class Value> auto EvictionSegmentedLRU<Key, Value>::victim_begin() const -> VictimIterator
{
    return VictimIterator{m_probation_list.rbegin(), m_probation_list.rend(), m_protected_list.rbegin()};
}

template<class Key, class Value> auto EvictionSegmentedLRU<Key, Value>::victim_end() const -> VictimIterator
{
    return VictimIterator{m_probation_list.rend(), m_probation_list.rend(), m_protected_list.rend()};
}

template<class Key, class Value> bool EvictionSegmentedLRU<Key, Value>::move_to_protected(const Key& key)
{
    auto probation_node_it = m_probation_nodes.find(key);
    if (probation_node_it == m_probation_nodes.end()) {
        return false;
    }

    m_protected_list.splice(m_protected_list.begin(), m_probation_list, probation_node_it->second);
    m_probation_nodes.erase(key);
    m_protected_nodes.emplace(key, m_protected_list.begin());
    return true;
}

template<class Key, class Value> bool EvictionSegmentedLRU<Key, Value>::pop_to_probation()
{
    if (m_protected_list.empty()) {
        return false;
    }

    m_probation_list.splice(m_probation_list.begin(), m_protected_list, --m_protected_list.end());
    m_protected_nodes.erase(*m_probation_list.begin());
    m_probation_nodes.emplace(*m_probation_list.begin(), m_probation_list.begin());
    return true;
}

}  // namespace cachemere::policy
