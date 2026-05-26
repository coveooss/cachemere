#pragma once

#include <cstdint>

#ifdef _WIN32
#    pragma warning(push)
#    pragma warning(disable : 4244)
#    pragma warning(disable : 4018)
#endif

#include <boost/dynamic_bitset.hpp>

#ifdef _WIN32
#    pragma warning(pop)
#endif

#include <cachemere/detail/traits.h>

#include "bloom_filter_math.h"
#include "hash_mixer.h"

namespace cachemere::policy::detail {

/// @brief Probabilistic data structure for representing sets in a space-efficient format.
/// @details A bloom filter is a constant-sized data structure, which means that insertions will never make
///          the filter allocate more memory. However, too many inserts will severly impact the accuracy of
///          filter membership tests.
/// @tparam ItemHash Functor used for hashing the items inserted in the set.
template<typename ItemHash> class BloomFilter
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
    BloomFilter(uint32_t cardinality)
     : m_cardinality{cardinality},
       m_filter{optimal_filter_size(cardinality), false},
       m_nb_hashes{optimal_nb_of_hash_functions(cardinality, m_filter.size())}
    {
    }

    /// @brief Add an item to the filter.
    /// @param item The item to insert.
    template<typename ItemKey> void add(const ItemKey& item)
    {
        HashMixer<ItemKey, ItemHash> mixer{item, m_filter.size()};

        for (size_t i = 0; i < m_nb_hashes; ++i) {
            const size_t filter_idx = mixer();
            assert(filter_idx < m_filter.size());

            m_filter.set(filter_idx);
        }
    }

    /// @brief Clear the filter while keeping the allocated memory.
    void clear()
    {
        m_filter.reset();
    }

    /// @brief Test membership of the specified item.
    /// @details A bloom filter can return false positives, but not false negatives.
    ///          This method returning `true` only means that the set _might_ contain the specified item,
    ///          while a return value of `false` means that the set _certainly_ does not contain the specified item.
    /// @param item The item to test.
    template<typename ItemKey> [[nodiscard]] bool maybe_contains(const ItemKey& item) const
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

    /// @brief Get an estimate of the memory consumption of the filter.
    /// @return The memory used by the filter, in bytes.
    [[nodiscard]] size_t memory_used() const noexcept
    {
        return m_filter.num_blocks() * sizeof(BitsetBlock) + sizeof(m_nb_hashes);
    }

    /// @brief Get the saturation of the filter.
    /// @details As filter saturations increases, so will the probability of false positives.
    ///          A filter saturation of 1.0 means that all underlying bits are set to `1`, so every call to `maybe_contains` will return `true`.
    ///          Increasing the filter cardinality will slow down the saturation of the filter, at the cost of using more memory.
    /// @return The saturation of the filter, as a fraction
    [[nodiscard]] double saturation() const noexcept
    {
        assert(m_filter.size() > 0);
        return static_cast<double>(m_filter.count()) / static_cast<double>(m_filter.size());
    }

private:
    using BitsetBlock = uint8_t;
    using BitSet      = boost::dynamic_bitset<BitsetBlock>;

    uint32_t m_cardinality;
    BitSet   m_filter;
    uint32_t m_nb_hashes;
};

}  // namespace cachemere::policy::detail
