#pragma once

#include <concepts>
#include <cstddef>
#include <utility>

#include "detail/traits.h"
#include "detail/transparent_eq.h"
#include "item.h"

namespace cachemere {

/// @brief Constraint for types usable as cache keys.
template<typename T>
concept CacheKey = std::movable<T> && std::move_constructible<T> && std::assignable_from<T&, T&&> && requires(T t) {
    { t == t } -> std::convertible_to<bool>;
};

/// @brief Constraint for hash functors usable with a specific value type.
template<typename T, typename V>
concept HasherFor = std::default_initializable<T> && requires(T t, V v) {
    { t(v) } -> std::convertible_to<size_t>;
};

/// @brief Constraint for key-like lookup values usable with a cache key and hasher.
template<typename KeyView, typename Key, typename KeyHash>
concept LookupKeyFor = HasherFor<KeyHash, KeyView> && requires(KeyView key_view, Key key) {
    { detail::TransparentEq<Key>{}(key, key_view) } -> std::same_as<bool>;
};

/// @brief Constraint for measurement functors usable with a specific value type.
template<typename T, typename V>
concept MeasureFor = std::default_initializable<T> && requires(T t, V v) {
    { t(v) } -> std::convertible_to<size_t>;
};

/// @brief Constraint for factory functors used to create a value from a key.
template<typename T, typename K, typename V>
concept FactoryFn = requires(T t, K key) {
    { t(key) } -> std::convertible_to<V>;
};

/// @brief Constraint for containers that can collect cache key/value pairs.
template<typename T, typename... Args>
concept CacheContainer = detail::traits::SequenceContainer<T, Args...> || detail::traits::AssociativeContainer<T, Args...>;

/// @brief Constraint for cache predicates accepting a key and value.
template<typename T, typename K, typename V>
concept PredicateFn = requires(T t, K key, V value) {
    { t(key, value) } -> std::same_as<bool>;
};

/// @brief Constraint for cache visitors accepting a key and value.
template<typename T, typename K, typename V>
concept UnaryFn = requires(T t, K key, V value) {
    { t(key, value) } -> std::same_as<void>;
};

}  // namespace cachemere
