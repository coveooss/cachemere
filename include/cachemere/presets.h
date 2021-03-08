#ifndef CACHEMERE_PRESETS_H
#define CACHEMERE_PRESETS_H

#include "cache.h"
#include "measurement.h"

#include "policy/eviction_lru.h"
#include "policy/eviction_segmented_lru.h"
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

}  // namespace cachemere::presets

#endif
