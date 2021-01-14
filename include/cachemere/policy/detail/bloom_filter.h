#ifndef CACHEMERE_BLOOM_FILTER_H
#define CACHEMERE_BLOOM_FILTER_H

#include <boost/dynamic_bitset.hpp>
#include <cstdint>
#include <functional>
#include <random>

#include "hash_mixer.h"

namespace cachemere::policy::detail {

/// @brief Probabilistic datastructure for representing sets in a space-efficient format.
/// @details A bloom filter is a constant-sized datastructure, which means that insertions will never make
///          the filter allocate more memory. However, too many inserts will severly impact the accuracy of
///          filter membership tests.
///          TODO: Document more.
/// @tparam Key The type of the items that will be inserted into the set.
/// @tparam KeyHash Functor used for hashing the keys inserted in the set.
template<typename Key, typename KeyHash = std::hash<Key>> class BloomFilter
{
public:
    /// @brief Constructor.
    /// @details To use this datastructure at its full potential, it's very important to have a good estimate
    ///          for the cardinality of the set to be inserted.
    ///
    ///          Having an estimate much higher than the real cardinality will result in excessive memory usage,
    ///          while having an estimate that is too low will drastically reduce the accuracy of the filter.
    ///
    /// @param cardinality The expected cardinality of the set to be inserted in the filter.
    BloomFilter(uint32_t cardinality);

    /// @brief Add an item to the filter.
    void add(const Key& key);

    /// @brief Clear the filter while keeping the allocated memory.
    void clear();

    /// @brief Test membership of the specified key.
    /// @details A bloom filter can return false positives, but not false negatives.
    ///          This method returning `true` only means that the set _might_ contain the specified key,
    ///          while a return value of `false` means that the set _certainly_ does not contain the specified key.
    /// @param key The key to test.
    [[nodiscard]] bool maybe_contains(const Key& key) const;

    /// @brief Get the amount of memory used by the filter, in bytes.
    [[nodiscard]] size_t memory_used() const noexcept;

    /// @brief Get the saturation of the filter, as a fraction.
    /// @details As filter saturations increases, so will the probability of false positives.
    ///          A filter saturation of 1.0 means that all underlying bits are set to `1`, so every call to `maybe_contains` will return `true`.
    ///          Increasing the filter cardinality will slow down the saturation of the filter, at the cost of using more memory.
    [[nodiscard]] double saturation() const noexcept;

private:
    using Mixer       = HashMixer<Key, KeyHash>;
    using BitsetBlock = uint8_t;
    using BitSet      = boost::dynamic_bitset<BitsetBlock>;

    uint32_t m_cardinality;
    BitSet   m_filter;
    uint32_t m_nb_hashes;
};

}  // namespace cachemere::policy::detail

#include "bloom_filter.hpp"

#endif
