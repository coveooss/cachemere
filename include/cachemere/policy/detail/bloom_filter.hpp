#include <cassert>

#include "bloom_filter_math.h"

namespace cachemere::policy::detail {

template<typename ItemHash>
BloomFilter<ItemHash>::BloomFilter(uint32_t cardinality)
 : m_cardinality{cardinality},
   m_filter{optimal_filter_size(cardinality), false},
   m_nb_hashes{optimal_nb_of_hash_functions(cardinality, m_filter.size())}
{
}

template<typename ItemHash> template<typename ItemKey> void BloomFilter<ItemHash>::add(const ItemKey& item)
{
    HashMixer<ItemKey, ItemHash> mixer{item, m_filter.size()};

    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t filter_idx = mixer();
        assert(filter_idx < m_filter.size());

        m_filter.set(filter_idx);
    }
}

template<typename ItemHash> void BloomFilter<ItemHash>::clear()
{
    m_filter.reset();
}

template<typename ItemHash> template<typename ItemKey> bool BloomFilter<ItemHash>::maybe_contains(const ItemKey& item) const
{
    HashMixer<ItemKey, ItemHash> mixer{item, m_filter.size()};

    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t filter_idx = mixer();
        assert(filter_idx < m_filter.size());

        if (!m_filter.test(filter_idx)) {
            return false;
        }
    }

    return true;
}

template<typename ItemHash> size_t BloomFilter<ItemHash>::memory_used() const noexcept
{
    return m_filter.num_blocks() * sizeof(BitsetBlock) + sizeof(m_nb_hashes);
}

template<typename ItemHash> double BloomFilter<ItemHash>::saturation() const noexcept
{
    assert(m_filter.size() > 0);
    return static_cast<double>(m_filter.count()) / static_cast<double>(m_filter.size());
}

}  // namespace cachemere::policy::detail
