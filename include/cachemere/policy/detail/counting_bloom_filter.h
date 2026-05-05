#pragma once

#include <cassert>
#include <limits>
#include <vector>

#include <absl/hash/hash.h>

#include "bloom_filter_math.h"
#include "hash_mixer.h"

namespace cachemere::policy::detail {

/// @brief Space-efficient probabilistic data structure to estimate the number of times
///        an item was inserted in a set.
/// @details A counting bloom filter is a constant-sized data structure, which means that insertions will never
///          make the filter allocate more memory. However, too many inserts will severely impact the accuracy
///          of counter estimates.
template<typename ItemHash> class CountingBloomFilter
{
public:
    /// @brief Constructor.
    /// @details To use this data structure at its full potential, it's very important to have a good estimate
    ///          for the cardinality of the set to be inserted.
    ///
    /// @warning Having an estimate much higher than the real cardinality will result in excessive memory usage,
    ///          while having an estimate that is too low will drastically reduce the accuracy of the filter.
    ///
    /// @param cardinality The expected cardinality of the set to be inserted in the filter.
    CountingBloomFilter(uint32_t cardinality);

    /// @brief Increment the count for a given item by one.
    /// @param item The item of the counter to increment.
    template<typename ItemKey> void add(const ItemKey& item);

    /// @brief Clear the filter while keeping the allocated memory.
    void clear();

    /// @brief Divide counter values by two.
    /// @details If the user of this filter doesn't rely on the absolute value of the counters, calling this
    ///          regularly will decrease the saturation of the filter by removing items that are very
    ///          rarely seen.
    void decay();

    /// @brief Get the counter estimate for a given item.
    /// @param item The item for which to estimate the count.
    /// @details Similarly to the way a bloom filter can return false positives, but not false negatives,
    ///          the counter estimation produced by a counting bloom filter is actually an upper bound of
    ///          the real counter value - the real count is guaranteed to be less or equal to the estimate
    ///          returned.
    /// @return The counter estimate for the item.
    template<typename ItemKey> [[nodiscard]] uint32_t estimate(const ItemKey& item) const;

    /// @brief Get the cardinality of the filter.
    /// @return The cardinality of the filter, as configured.
    [[nodiscard]] uint32_t cardinality() const noexcept;

    /// @brief Get an estimate of the memory consumption of the filter.
    /// @return The memory used by the filter, in bytes.
    [[nodiscard]] size_t memory_used() const noexcept;

    /// @brief Get the saturation of the filter.
    /// @details As filter saturations increases, so will the probability of false positives.
    ///          A filter saturation of 1.0 means that all underlying counters are non-zero, so every call to `estimate` will return
    ///          a counter value greater than 1.
    ///          Increasing the filter cardinality will slow down the saturation of the filter, at the cost of using more memory.
    /// @return  The saturation of the filter, as a fraction.
    [[nodiscard]] double saturation() const noexcept;

private:
    uint32_t              m_cardinality;
    std::vector<uint32_t> m_filter;
    uint32_t              m_nb_hashes;
    uint32_t              m_nb_nonzero = 0;
};

template<typename ItemHash>
CountingBloomFilter<ItemHash>::CountingBloomFilter(uint32_t cardinality)
 : m_cardinality{cardinality},
   m_filter(optimal_filter_size(cardinality), 0),
   m_nb_hashes{optimal_nb_of_hash_functions(cardinality, m_filter.size())}
{
}

template<typename ItemHash> template<typename ItemKey> void CountingBloomFilter<ItemHash>::add(const ItemKey& item)
{
    HashMixer<ItemKey, ItemHash> mixer{item, m_filter.size()};

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

template<typename ItemHash> void CountingBloomFilter<ItemHash>::clear()
{
    std::ranges::fill(m_filter.begin(), m_filter.end(), 0);
    m_nb_nonzero = 0;
}

template<typename ItemHash> void CountingBloomFilter<ItemHash>::decay()
{
    for (auto& counter : m_filter) {
        if (counter == 1) {
            --m_nb_nonzero;
        }
        counter /= 2;
    }
}

template<typename ItemHash> template<typename ItemKey> uint32_t CountingBloomFilter<ItemHash>::estimate(const ItemKey& item) const
{
    assert(m_nb_hashes > 0);

    HashMixer<ItemKey, ItemHash> mixer{item, m_filter.size()};

    uint32_t minimum_val = std::numeric_limits<uint32_t>::max();
    for (size_t i = 0; i < m_nb_hashes; ++i) {
        const size_t idx = mixer();
        assert(idx < m_filter.size());

        minimum_val = std::min(m_filter[idx], minimum_val);
    }

    return minimum_val;
}

template<typename ItemHash> uint32_t CountingBloomFilter<ItemHash>::cardinality() const noexcept
{
    return m_cardinality;
}

template<typename ItemHash> size_t CountingBloomFilter<ItemHash>::memory_used() const noexcept
{
    // Note:
    //  The amount of memory could be reduced by using variable-length counters.
    //  Since we know that reset is triggered once we reach a count of
    //  m_Cardinality, we could use the smallest numeric type that holds
    //  m_Cardinality for the internal counters.
    return m_filter.size() * sizeof(uint32_t) + sizeof(m_cardinality) + sizeof(m_nb_hashes) + sizeof(m_nb_nonzero);
}

template<typename ItemHash> double CountingBloomFilter<ItemHash>::saturation() const noexcept
{
    assert(m_filter.size() > 0);
    return static_cast<double>(m_nb_nonzero) / static_cast<double>(m_filter.size());
}

}  // namespace cachemere::policy::detail
