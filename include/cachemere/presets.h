#ifndef CACHEMERE_PRESETS_H
#define CACHEMERE_PRESETS_H

#include "cache.h"
#include "measurement.h"

#include "policy/eviction_lru.h"
#include "policy/eviction_segmented_lru.h"
#include "policy/eviction_gdsf.h"
#include "policy/insertion_always.h"
#include "policy/insertion_tinylfu.h"

/// @brief Frequently-used cache presets.
namespace cachemere::presets {

/// @brief Least-Recently-Used Cache.
/// @details Uses a linked list to order items from hottest (most recently accessed) to coldest (least recently accessed).
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
template<typename Key, typename Value, typename MeasureValue = measurement::Size<Value>, typename MeasureKey = measurement::Size<Key>>
using LRUCache = Cache<Key, Value, policy::InsertionAlways, policy::EvictionLRU, MeasureValue, MeasureKey>;

/// @brief TinyLFU Cache.
/// @details Uses a combination of frequency sketches to gather a decent estimate of the access frequency of most keys.
///          Uses this estimate to decide which item should be held in cache over another.
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
template<typename Key, typename Value, typename MeasureValue = measurement::Size<Value>, typename MeasureKey = measurement::Size<Key>>
using TinyLFUCache = Cache<Key, Value, policy::InsertionTinyLFU, policy::EvictionSegmentedLRU, MeasureValue, MeasureKey>;

namespace detail {

// Template helper to allow partial specialization of Cost.
template<typename Key, typename Value, typename Cost, typename MeasureValue = measurement::Size<Value>, typename MeasureKey = measurement::Size<Key>>
class SpecializedGDSFCache
{
private:
    template<typename K, typename V> using MyGDSF = policy::EvictionGDSF<K, V, Cost>;

public:
    using type = Cache<Key, Value, policy::InsertionAlways, MyGDSF, MeasureValue, MeasureKey>;
};

}  // namespace detail

/// @brief Custom-Cost Cache.
/// @details The use of this cache should be favored in scenarios where the cost of a cache miss varies greatly from one item to the next.
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam Cost A functor taking a `const Item<Key, Value>&` returning the cost to load this item in cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
template<typename Key, typename Value, typename Cost, typename MeasureValue = measurement::Size<Value>, typename MeasureKey = measurement::Size<Key>>
using CustomCostCache = typename detail::SpecializedGDSFCache<Key, Value, Cost, MeasureValue, MeasureKey>::type;

}  // namespace cachemere::presets

#endif
