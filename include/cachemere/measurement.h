#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

/// @brief Utilities for measuring cached items.
namespace cachemere::measurement {

namespace detail {

// Utility functions to allow passing both object and pointers to objects
// to the measurement functors.
template<typename T, typename F>
decltype(auto) with_deref_maybe(const T& object, F&& fn)
{
    return std::forward<F>(fn)(object);
}

template<typename T, typename F>
decltype(auto) with_deref_maybe(T* object, F&& fn)
{
    assert(object != nullptr);
    return std::forward<F>(fn)(*object);
}

template<typename T, typename F>
decltype(auto) with_deref_maybe(const std::shared_ptr<T>& object, F&& fn)
{
    assert(object != nullptr);
    return std::forward<F>(fn)(*object);
}

template<typename T, typename D, typename F>
decltype(auto) with_deref_maybe(const std::unique_ptr<T, D>& object, F&& fn)
{
    assert(object != nullptr);
    return std::forward<F>(fn)(*object);
}

}  // namespace detail

/// @brief Get the size of an object via a user-defined `size()` method.
/// @details Raw pointers, `std::shared_ptr`, and `std::unique_ptr` are dereferenced before calling `size()`.
///          Pointer-like arguments must not be null.
template<typename T> struct Size {
    /// @brief Measure an object or pointer-like wrapper around an object.
    /// @param object The object to measure.
    /// @return The result of calling `size()` on the referenced object.
    template<typename V> size_t operator()(const V& object) const;
};

/// @brief Get the size of an object via `sizeof()`.
/// @details Returns `sizeof(T)` regardless of the wrapper type passed to `operator()`.
template<typename T> struct SizeOf {
    /// @brief Return `sizeof(T)`.
    /// @param object Unused measurement input.
    /// @return `sizeof(T)`.
    template<typename V> size_t operator()(const V& object) const;
};

/// @brief Get the size of an object via a user-defined `capacity()` method.
/// @details Raw pointers, `std::shared_ptr`, and `std::unique_ptr` are dereferenced before calling `capacity()`.
///          Small capacities are rounded up to better approximate typical dynamic allocation behavior.
template<typename T> struct CapacityDynamicallyAllocated {
public:
    /// @brief Measure the dynamically allocated capacity of an object.
    /// @param object The object to measure.
    /// @return The object's capacity, rounded up for small allocations.
    template<typename V> size_t operator()(const V& object) const;

private:
    [[nodiscard]] size_t round_up(const size_t value) const;
};


template<typename T> template<typename V> size_t Size<T>::operator()(const V& object) const
{
    return detail::with_deref_maybe(object, [](const auto& obj) {
        return obj.size();
    });
}

template<typename T> template<typename V> size_t SizeOf<T>::operator()(const V& /* object */) const
{
    return sizeof(T);
}

template<typename T> template<typename V> size_t CapacityDynamicallyAllocated<T>::operator()(const V& object) const
{
    size_t capacity = detail::with_deref_maybe(object, [](const auto& obj) {
        return obj.capacity();
    });

    if (capacity < 1024) {
        capacity = std::max(static_cast<size_t>(16), round_up(capacity));
    }

    return capacity;
}

template<typename T> size_t CapacityDynamicallyAllocated<T>::round_up(const size_t value) const
{
    return ((value + sizeof(void*) - 1) / sizeof(void*)) * sizeof(void*);
}

}  // namespace cachemere::measurement
