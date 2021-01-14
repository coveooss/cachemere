#ifndef CACHEMERE_HASHMIXER_H
#define CACHEMERE_HASHMIXER_H

#include <cstdint>
#include <random>

namespace cachemere::policy::detail {

template<typename Key, typename KeyHash> class HashMixer : private KeyHash
{
public:
    HashMixer(const Key& key, size_t value_range);
    [[nodiscard]] size_t operator()();

private:
    std::minstd_rand m_rng;
    size_t           m_value_range;
};

}  // namespace cachemere::policy::detail

#include "hash_mixer.hpp"

#endif
