#pragma once

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <cachemere/item.h>

namespace cachemere::detail::traits {

template<typename T> struct is_basic_string : std::false_type {
};

template<typename CharT, typename Traits, typename Alloc> struct is_basic_string<std::basic_string<CharT, Traits, Alloc>> : std::true_type {
};

template<typename T>
concept BasicString = is_basic_string<std::remove_cvref_t<T>>::value;

template<class T> struct is_basic_string_view : std::false_type {
};

template<class CharT, class Traits> struct is_basic_string_view<std::basic_string_view<CharT, Traits>> : std::true_type {
};

template<class T>
concept BasicStringView = is_basic_string_view<std::remove_cvref_t<T>>::value;

template<typename T, typename... Args>
concept SequenceContainer = requires(T t, Args... args) {
    { t.emplace_back(args...) };
};

template<typename T, typename... Args>
concept AssociativeContainer = requires(T t, Args... args) {
    { t.emplace(args...) };
};

template<typename T>
concept ReservableContainer = requires(T t) {
    { t.reserve(std::declval<size_t>()) };
    { t.size() } -> std::convertible_to<size_t>;
};

// Traits for cache event handlers.
namespace event {

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept HasOnInsert = requires(P<K, KH, V> policy, K key, Item<V> item) {
    { policy.on_insert(key, item) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept HasOnUpdate = requires(P<K, KH, V> policy, K key, Item<V> old_item, Item<V> new_item) {
    { policy.on_update(key, old_item, new_item) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept HasOnCacheHit = requires(P<K, KH, V> policy, K key, Item<V> item) {
    { policy.on_cache_hit(key, item) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept HasOnCacheMiss = requires(P<K, KH, V> policy, K key) {
    { policy.on_cache_miss(key) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept HasOnEvict = requires(P<K, KH, V> policy, K key, Item<V> item) {
    { policy.on_evict(key, item) };
};

}  // namespace event

}  // namespace cachemere::detail::traits
