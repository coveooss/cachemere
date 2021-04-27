#ifndef CACHEMERE_MEASUREMENT_H
#define CACHEMERE_MEASUREMENT_H

#include <cstdint>

/// @brief Utilities for measuring cached items.
namespace cachemere::measurement {

/// @brief Get the size of an object via a user-defined `size()` method.
template<typename T> struct Size {
    template<typename V> size_t operator()(const V& object) const;
};

/// @brief Get the size of an object via `sizeof()`.
template<typename T> struct SizeOf {
    template<typename V> size_t operator()(const V& object) const;
};

/// @brief Get the size of an object via a user-defined `capacity()` method.
template<typename T> struct CapacityDynamicallyAllocated {
public:
    template<typename V> size_t operator()(const V& object) const;

private:
    [[nodiscard]] size_t round_up(const size_t value) const;
};

}  // namespace cachemere::measurement

#include "measurement.hpp"

#endif
