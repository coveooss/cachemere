#ifndef CACHEMERE_HASHMIXER_H
#define CACHEMERE_HASHMIXER_H

#include <cstdint>
#include <random>

namespace cachemere::policy::detail {

/// @brief Functor used for generating a uniform sequence of numbers in a given value range for a given key.
/// @tparam Key The type of the key to be used as seed.
/// @tparam KeyHash The functor to use for turning the provided key into a seed for the internal
///                 pseudo-random number generator.
template<typename Key, typename KeyHash> class HashMixer : private KeyHash
{
public:
    /// @brief Constructor.
    /// @param key The key to use to seed this instance.
    /// @param value_range The upper bound of the value range.
    ///                    This mixer will return values in the range `[0, value_range)`.
    HashMixer(const Key& key, size_t value_range);

    /// @brief Generate the next value in the random sequence.
    /// @return The next value in the sequence.
    [[nodiscard]] size_t operator()();

private:
    std::minstd_rand m_rng;
    size_t           m_value_range;
};

}  // namespace cachemere::policy::detail

#include "hash_mixer.hpp"

#endif
