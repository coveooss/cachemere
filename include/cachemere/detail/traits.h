#ifndef CACHEMERE_TRAITS_H
#define CACHEMERE_TRAITS_H

#include "cachemere/item.h"

namespace cachemere::detail::traits {

// Traits for STL types.
namespace stl {

template<typename T>
constexpr auto has_reserve = boost::hana::is_valid([](auto& t) -> decltype(boost::hana::traits::declval(t).reserve(std::declval<size_t>())) {
})(boost::hana::type_c<T>);

template<typename T>
constexpr auto has_size = boost::hana::is_valid([](auto& t) -> decltype(boost::hana::traits::declval(t).size()) {})(boost::hana::type_c<T>);

template<typename T, typename... Args>
constexpr auto has_emplace_back = boost::hana::is_valid([](auto& t) -> decltype(boost::hana::traits::declval(t).emplace_back(std::declval<Args>()...)) {
})(boost::hana::type_c<T>);

}  // namespace stl

// Traits for cache event handlers.
namespace event {

template<typename K, typename V, template<class, class> typename P>
constexpr auto has_on_insert = boost::hana::is_valid(
    [](auto& policy_t, auto& key_t, auto& item_t) -> decltype(boost::hana::traits::declval(policy_t).on_insert(boost::hana::traits::declval(key_t),
                                                                                                               boost::hana::traits::declval(item_t))) {
    })(boost::hana::type_c<P<K, V>>, boost::hana::type_c<K>, boost::hana::type_c<Item<V>>);

template<typename K, typename V, template<class, class> typename P>
constexpr auto has_on_update = boost::hana::is_valid(
    [](auto& policy_t, auto& key_t, auto& item_t) -> decltype(boost::hana::traits::declval(policy_t).on_update(boost::hana::traits::declval(key_t),
                                                                                                               boost::hana::traits::declval(item_t))) {
    })(boost::hana::type_c<P<K, V>>, boost::hana::type_c<K>, boost::hana::type_c<Item<V>>);

template<typename K, typename V, template<class, class> typename P>
constexpr auto has_on_cachehit = boost::hana::is_valid(
    [](auto& policy_t, auto& key_t, auto& item_t) -> decltype(boost::hana::traits::declval(policy_t).on_cache_hit(boost::hana::traits::declval(key_t),
                                                                                                                  boost::hana::traits::declval(item_t))) {
    })(boost::hana::type_c<P<K, V>>, boost::hana::type_c<K>, boost::hana::type_c<Item<V>>);

template<typename K, typename V, template<class, class> typename P>
constexpr auto has_on_cachemiss = boost::hana::is_valid(
    [](auto& policy_t, auto& item_t) -> decltype(boost::hana::traits::declval(policy_t).on_cache_miss(boost::hana::traits::declval(item_t))) {
    })(boost::hana::type_c<P<K, V>>, boost::hana::type_c<K>);

template<typename K, typename V, template<class, class> typename P>
constexpr auto has_on_evict =
    boost::hana::is_valid([](auto& policy_t, auto& item_t) -> decltype(boost::hana::traits::declval(policy_t).on_evict(boost::hana::traits::declval(item_t))) {
    })(boost::hana::type_c<P<K, V>>, boost::hana::type_c<K>);

}  // namespace event

}  // namespace cachemere::detail::traits

#endif
