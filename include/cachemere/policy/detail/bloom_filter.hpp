#include <cassert>

#include "bloom_filter_math.h"

namespace cachemere::policy::detail {

template<typename Key, typename KeyHash>
BloomFilter<Key, KeyHash>::BloomFilter(uint32_t cardinality)
 : m_cardinality{cardinality},
   m_filter{optimal_filter_size(cardinality), false},
   m_nb_hashes{optimal_nb_of_hash_functions(cardinality, m_filter.size())}
{
}

template<typename Key, typename KeyHash> void BloomFilter<Key, KeyHash>::add(const Key& key)
{
    Mixer mixer{key, m_filter.size()};

    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t filter_idx = mixer();
        assert(filter_idx < m_filter.size());

        m_filter.set(filter_idx);
    }
}

template<typename Key, typename KeyHash> void BloomFilter<Key, KeyHash>::clear()
{
    m_filter.reset();
}

template<typename Key, typename KeyHash> bool BloomFilter<Key, KeyHash>::maybe_contains(const Key& key) const
{
    Mixer mixer{key, m_filter.size()};

    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t filter_idx = mixer();
        assert(filter_idx < m_filter.size());

        if (!m_filter.test(filter_idx)) {
            return false;
        }
    }

    return true;
}

template<typename Key, typename KeyHash> size_t BloomFilter<Key, KeyHash>::memory_used() const noexcept
{
    return m_filter.num_blocks() * sizeof(BitsetBlock) + sizeof(m_nb_hashes);
}

template<typename Key, typename KeyHash> double BloomFilter<Key, KeyHash>::saturation() const noexcept
{
    assert(m_filter.size() > 0);
    return static_cast<double>(m_filter.count()) / static_cast<double>(m_filter.size());
}

}  // namespace cachemere::policy::detail
