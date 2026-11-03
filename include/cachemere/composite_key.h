#pragma once

#include <concepts>
#include <tuple>
#include <utility>

#include "detail/traits.h"

namespace cachemere {

/// @brief Composite key made of several key components.
/// @details Stores each component by value and exposes equality and Abseil hashing support for the resulting aggregate key.
/// @tparam Keys The component types making up the key.
template<typename... Keys> class CompositeKey
{
public:
    /// @brief Construct a composite key from its components.
    /// @param keys The key components to store.
    template<typename... OtherKeys>
        requires(sizeof...(OtherKeys) == sizeof...(Keys) && (std::constructible_from<Keys, OtherKeys &&> && ...))
    CompositeKey(OtherKeys&&... keys) : m_keys(std::forward<OtherKeys>(keys)...)
    {
    }

    /// @brief Compare two composite keys component-wise.
    /// @param other The key to compare against.
    /// @return Whether every component compares equal.
    template<typename... OtherKeys> bool operator==(const CompositeKey<OtherKeys...>& other) const
    {
        return m_keys == other.m_keys;
    }

    /// @brief Hook used by Abseil hashing to hash the full composite key.
    /// @param h The incremental hash state.
    /// @param key The composite key to hash.
    /// @return The updated hash state.
    template<typename H, typename... S> friend H AbslHashValue(H h, const CompositeKey& key)
    {
        std::apply([&](const auto&... keys) { (HashTuple(h, keys), ...); }, key.m_keys);

        return h;
    }

private:
    template<typename...> friend class CompositeKey;

    std::tuple<Keys...> m_keys;

    template<typename H, typename T> static void HashTuple(H& h, const T& key)
    {
        if constexpr (detail::traits::BasicString<T>) {
            h = H::combine_contiguous(std::move(h), key.c_str(), key.size());
        } else if constexpr (detail::traits::BasicStringView<T>) {
            h = H::combine_contiguous(std::move(h), key.data(), key.size());
        } else {
            h = H::combine(std::move(h), key);
        }
    }
};

}  // namespace cachemere
