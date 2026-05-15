#pragma once

#include <cachemere/item.h>

namespace cachemere::detail::traits {

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
