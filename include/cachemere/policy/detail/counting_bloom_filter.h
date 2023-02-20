#ifndef CACHEMERE_COUNTINGBLOOMFILTER_H
#define CACHEMERE_COUNTINGBLOOMFILTER_H

#include <functional>
#include <vector>

#include <absl/hash/hash.h>

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

}  // namespace cachemere::policy::detail

#include "counting_bloom_filter.hpp"

#endif
