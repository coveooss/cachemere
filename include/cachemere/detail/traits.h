#pragma once

#include <cachemere/item.h>

namespace cachemere::detail::traits {

// Traits for STL types.
namespace stl {

template<typename T>
concept has_reserve = requires(T t) {
    { t.reserve(std::declval<size_t>()) };
};

template<typename T>
concept has_size = requires(T t) {
    { t.size() } -> std::convertible_to<size_t>;
};

template<typename T, typename... Args>
concept has_emplace_back = requires(T t, Args... args) {
    { t.emplace_back(args...) };
};

}  // namespace stl

// Traits for cache event handlers.
namespace event {

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept has_on_insert = requires(P<K, KH, V> policy, K key, Item<V> item) {
    { policy.on_insert(key, item) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept has_on_update = requires(P<K, KH, V> policy, K key, Item<V> old_item, Item<V> new_item) {
    { policy.on_update(key, old_item, new_item) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept has_on_cachehit = requires(P<K, KH, V> policy, K key, Item<V> item) {
    { policy.on_cache_hit(key, item) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept has_on_cachemiss = requires(P<K, KH, V> policy, K key) {
    { policy.on_cache_miss(key) };
};

template<typename K, typename KH, typename V, template<class, class, class> typename P>
concept has_on_evict = requires(P<K, KH, V> policy, K key, Item<V> item) {
    { policy.on_evict(key, item) };
};

}  // namespace event

}  // namespace cachemere::detail::traits
