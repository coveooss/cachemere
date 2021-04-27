#include <cassert>

#include "bloom_filter_math.h"

namespace cachemere::policy::detail {

template<typename Item, typename ItemHash>
BloomFilter<Item, ItemHash>::BloomFilter(uint32_t cardinality)
 : m_cardinality{cardinality},
   m_filter{optimal_filter_size(cardinality), false},
   m_nb_hashes{optimal_nb_of_hash_functions(cardinality, m_filter.size())}
{
}

template<typename Item, typename ItemHash> void BloomFilter<Item, ItemHash>::add(const Item& item)
{
    Mixer mixer{item, m_filter.size()};

    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t filter_idx = mixer();
        assert(filter_idx < m_filter.size());

        m_filter.set(filter_idx);
    }
}

template<typename Item, typename ItemHash> void BloomFilter<Item, ItemHash>::clear()
{
    m_filter.reset();
}

template<typename Item, typename ItemHash> bool BloomFilter<Item, ItemHash>::maybe_contains(const Item& item) const
{
    Mixer mixer{item, m_filter.size()};

    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t filter_idx = mixer();
        assert(filter_idx < m_filter.size());

        if (!m_filter.test(filter_idx)) {
            return false;
        }
    }

    return true;
}

template<typename Item, typename ItemHash> size_t BloomFilter<Item, ItemHash>::memory_used() const noexcept
{
    return m_filter.num_blocks() * sizeof(BitsetBlock) + sizeof(m_nb_hashes);
}

template<typename Item, typename ItemHash> double BloomFilter<Item, ItemHash>::saturation() const noexcept
{
    assert(m_filter.size() > 0);
    return static_cast<double>(m_filter.count()) / static_cast<double>(m_filter.size());
}

}  // namespace cachemere::policy::detail
