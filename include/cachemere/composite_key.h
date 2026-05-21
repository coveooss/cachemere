#pragma once

#include <concepts>
#include <tuple>
#include <utility>

#include "detail/traits.h"

namespace cachemere {

template<typename... Keys> class CompositeKey
{
public:
    template<typename... OtherKeys>
        requires(sizeof...(OtherKeys) == sizeof...(Keys) && (std::constructible_from<Keys, OtherKeys &&> && ...))
    CompositeKey(OtherKeys&&... keys) : m_keys(std::forward<OtherKeys>(keys)...)
    {
    }

    template<typename... OtherKeys> bool operator==(const CompositeKey<OtherKeys...>& other) const
    {
        return m_keys == other.m_keys;
    }

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
