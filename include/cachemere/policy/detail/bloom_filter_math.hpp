#include <cassert>

namespace cachemere::policy::detail {

inline size_t optimal_filter_size(uint32_t cardinality) noexcept
{
    const static double multiplier     = log(0.01) / pow(log(2), 2);
    const double        ideal_set_size = -static_cast<double>(cardinality) * multiplier;

    assert(ideal_set_size > 1);
    return static_cast<size_t>(ideal_set_size);
}

inline uint32_t optimal_nb_of_hash_functions(uint32_t cardinality, size_t filter_size) noexcept
{
    const static double log_2     = log(2);
    const double        nb_hashes = (static_cast<double>(filter_size) / static_cast<double>(cardinality)) * log_2;

    assert(nb_hashes >= 1);
    return static_cast<uint32_t>(nb_hashes);
}

}  // namespace cachemere::policy::detail
