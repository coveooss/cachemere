#include <memory>

namespace cachemere::measurement {

namespace detail {

// Utility functions to allow passing both object and pointers to objects
// to the measurement functors.
template<typename T> inline const T& deref_maybe(const T& object)
{
    return object;
}
template<typename T> inline const T& deref_maybe(T* object)
{
    return *object;
}
template<typename T> inline const T& deref_maybe(const std::shared_ptr<T>& object)
{
    return *object;
}
template<typename T> inline const T& deref_maybe(const std::unique_ptr<T>& object)
{
    return *object;
}

}  // namespace detail

template<typename T> template<typename V> size_t Size<T>::operator()(const V& object) const
{
    return detail::deref_maybe(object).size();
}

template<typename T> template<typename V> size_t SizeOf<T>::operator()(const V& object) const
{
    return sizeof(T);
}

template<typename T> template<typename V> size_t CapacityDynamicallyAllocated<T>::operator()(const V& object) const
{
    size_t capacity = detail::deref_maybe(object).capacity();
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
