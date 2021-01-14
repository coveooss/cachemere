#include <cassert>
#include <limits>
#include <vector>

#include "bloom_filter_math.h"

namespace cachemere::policy::detail {

template<typename Key, typename KeyHash>
CountingBloomFilter<Key, KeyHash>::CountingBloomFilter(uint32_t cardinality)
 : m_cardinality{cardinality},
   m_filter(optimal_filter_size(cardinality), 0),
   m_nb_hashes{optimal_nb_of_hash_functions(cardinality, m_filter.size())}
{
}

template<typename Key, typename KeyHash> void CountingBloomFilter<Key, KeyHash>::add(const Key& key)
{
    Mixer mixer{key, m_filter.size()};

    std::vector<size_t> indices;
    indices.reserve(m_nb_hashes);

    uint32_t minimum_val = std::numeric_limits<uint32_t>::max();

    // Generate the indices and find the minimum.
    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t idx = mixer();
        assert(idx < m_filter.size());

        indices.push_back(idx);
        minimum_val = std::min(m_filter[idx], minimum_val);
    }

    // Increment all filter slots corresponding to the minimum value.
    const uint8_t is_zero_increment = minimum_val == 0 ? 1 : 0;
    for (const size_t idx : indices) {
        if (m_filter[idx] == minimum_val) {
            ++m_filter[idx];

            // We track the number of non-zero values in the filter.
            // This is used to compute the filter saturation.
            m_nb_nonzero += is_zero_increment;
        }
    }
}

template<typename Key, typename KeyHash> void CountingBloomFilter<Key, KeyHash>::clear()
{
    std::fill(m_filter.begin(), m_filter.end(), 0);
    m_nb_nonzero = 0;
}

template<typename Key, typename KeyHash> void CountingBloomFilter<Key, KeyHash>::decay()
{
    for (auto& counter : m_filter) {
        if (counter == 1) {
            --m_nb_nonzero;
        }
        counter /= 2;
    }
}

template<typename Key, typename KeyHash> uint32_t CountingBloomFilter<Key, KeyHash>::estimate(const Key& key) const
{
    assert(m_nb_hashes > 0);

    Mixer mixer{key, m_filter.size()};

    uint32_t minimum_val = std::numeric_limits<uint32_t>::max();
    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t idx = mixer();
        assert(idx < m_filter.size());

        minimum_val = std::min(m_filter[idx], minimum_val);
    }

    return minimum_val;
}

template<typename Key, typename KeyHash> uint32_t CountingBloomFilter<Key, KeyHash>::cardinality() const noexcept
{
    return m_cardinality;
}

template<typename Key, typename KeyHash> size_t CountingBloomFilter<Key, KeyHash>::memory_used() const noexcept
{
    // Note:
    //  The amount of memory could be reduced by using variable-length counters.
    //  Since we know that reset is triggered once we reach a count of
    //  m_Cardinality, we could use the smallest numeric type that holds
    //  m_Cardinality for the internal counters.
    return m_filter.size() * sizeof(uint32_t) + sizeof(m_cardinality) + sizeof(m_nb_hashes) + sizeof(m_nb_nonzero);
}

template<typename Key, typename KeyHash> double CountingBloomFilter<Key, KeyHash>::saturation() const noexcept
{
    assert(m_filter.size() > 0);
    return static_cast<double>(m_nb_nonzero) / static_cast<double>(m_filter.size());
}

}  // namespace cachemere::policy::detail
