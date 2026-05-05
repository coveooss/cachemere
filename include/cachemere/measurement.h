#pragma once

#include <memory>

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
