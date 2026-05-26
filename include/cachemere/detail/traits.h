#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <cachemere/item.h>
#include <cachemere/detail/transparent_eq.h>

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

template<typename T, typename... Args>
concept CacheContainer = SequenceContainer<T, Args...> || AssociativeContainer<T, Args...>;

template<typename T>
concept ReservableContainer = requires(T t) {
    { t.reserve(std::declval<size_t>()) };
    { t.size() } -> std::convertible_to<size_t>;
};

template<typename T, typename K, typename V>
concept FactoryFn = requires(T t, K key) {
    { t(key) } -> std::convertible_to<V>;
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

template<typename T>
concept Key = std::movable<T> && std::move_constructible<T> && std::assignable_from<T&, T&&> && requires(T t) {
    { t == t } -> std::convertible_to<bool>;
};

template<typename T, typename V>
concept HasherFor = requires(T t, V v) {
    { t(v) } -> std::convertible_to<size_t>;
};

template<typename KeyView, typename Key, typename KeyHash>
concept LookupKeyFor = HasherFor<KeyHash, KeyView> && requires(KeyHash hash, KeyView key_view, Key key) {
    { TransparentEq<Key>{}(key, key_view) } -> std::same_as<bool>;
};

template<typename T, typename V>
concept MeasureFor = requires(T t, V v) {
    { t(v) } -> std::convertible_to<size_t>;
};

template<typename T, typename Key, typename Value>
concept PredicateFn = requires(T t, Key key, Value value) {
    { t(key, value) } -> std::same_as<bool>;
};

template<typename T, typename Key, typename Value>
concept UnaryFn = requires(T t, Key key, Value value) {
    { t(key, value) } -> std::same_as<void>;
};

}  // namespace cachemere::detail::traits
