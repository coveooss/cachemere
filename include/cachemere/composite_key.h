#pragma once

#include <concepts>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "detail/traits.h"

namespace cachemere {

/// @brief Composite key made of several key components.
/// @details Stores each component by value and exposes equality and Abseil hashing support for the resulting aggregate key.
///          Strings, string views and C strings are hashed by content, so keys made of any of them hash the same when they compare equal.
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

    /// @brief Construct a composite key from a composite key with different component types.
    /// @details For instance, builds an owning `CompositeKey<std::string>` from a `CompositeKey<std::string_view>` view.
    /// @param other The key whose components to convert.
    template<typename... OtherKeys>
        requires(sizeof...(OtherKeys) == sizeof...(Keys) && (std::constructible_from<Keys, const OtherKeys&> && ...))
    explicit CompositeKey(const CompositeKey<OtherKeys...>& other) : m_keys(std::make_from_tuple<std::tuple<Keys...>>(other.m_keys))
    {
    }

    /// @brief Compare two composite keys component-wise.
    /// @param other The key to compare against.
    /// @return Whether every component compares equal.
    template<typename... OtherKeys> bool operator==(const CompositeKey<OtherKeys...>& other) const
    {
        return m_keys == other.m_keys;
    }

    /// @brief Get the size of the key, as measured by `measurement::Size`.
    /// @details Sums the `size()` of the components that have one, the length of C strings, and the `sizeof()` of the other components.
    /// @return The size of the key.
    [[nodiscard]] size_t size() const
    {
        return std::apply([](const auto&... keys) { return (size_t{0} + ... + ComponentSize(keys)); }, m_keys);
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
        if constexpr (detail::traits::BasicString<T> || detail::traits::BasicStringView<T>) {
            // Hash the length after the characters, so that keys such as ("", "x") and ("x", "") don't collide.
            h = H::combine(H::combine_contiguous(std::move(h), key.data(), key.size()), key.size());
        } else if constexpr (detail::traits::CString<T>) {
            using View = std::basic_string_view<std::remove_cv_t<std::remove_pointer_t<T>>>;
            HashTuple(h, key == nullptr ? View{} : View{key});
        } else {
            h = H::combine(std::move(h), key);
        }
    }

    template<typename T> static size_t ComponentSize(const T& key)
    {
        if constexpr (requires {
                          { key.size() } -> std::convertible_to<size_t>;
                      }) {
            return key.size();
        } else if constexpr (detail::traits::CString<T>) {
            return key == nullptr ? 0 : std::basic_string_view<std::remove_cv_t<std::remove_pointer_t<T>>>{key}.size();
        } else {
            return sizeof(T);
        }
    }
};

}  // namespace cachemere
