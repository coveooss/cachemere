#ifndef CACHEMERE_COUNTINGBLOOMFILTER_H
#define CACHEMERE_COUNTINGBLOOMFILTER_H

#include <functional>
#include <vector>

#include "hash_mixer.h"

namespace cachemere::policy::detail {

template<typename Key, typename KeyHash = std::hash<Key>> class CountingBloomFilter
{
public:
    CountingBloomFilter(uint32_t cardinality);

    void                   add(const Key& key);
    void                   clear();
    void                   decay();
    [[nodiscard]] uint32_t estimate(const Key& key) const;

    [[nodiscard]] uint32_t cardinality() const noexcept;
    [[nodiscard]] size_t   memory_used() const noexcept;
    [[nodiscard]] double   saturation() const noexcept;

private:
    using Mixer = HashMixer<Key, KeyHash>;

    uint32_t              m_cardinality;
    std::vector<uint32_t> m_filter;
    uint32_t              m_nb_hashes;
    uint32_t              m_nb_nonzero = 0;
};

}  // namespace cachemere::policy::detail

#include "counting_bloom_filter.hpp"

#endif
