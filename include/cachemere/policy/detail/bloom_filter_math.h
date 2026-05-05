#pragma once

#include <cstddef>
#include <cstdint>

namespace cachemere::policy::detail {

[[nodiscard]] size_t   optimal_filter_size(const uint32_t cardinality) noexcept;
[[nodiscard]] uint32_t optimal_nb_of_hash_functions(const uint32_t cardinality, const size_t filter_size) noexcept;

}  // namespace cachemere::policy::detail

#include "bloom_filter_math.hpp"
