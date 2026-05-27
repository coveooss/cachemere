#pragma once

#include <absl/hash/hash.h>

#include "cache.h"
#include "measurement.h"

#include "policy/bind.h"
#include "policy/constraint_count.h"
#include "policy/constraint_memory.h"
#include "policy/eviction_lru.h"
#include "policy/eviction_segmented_lru.h"
#include "policy/eviction_gdsf.h"
#include "policy/insertion_always.h"
#include "policy/insertion_tinylfu.h"

/// @brief Frequently-used cache presets.
namespace cachemere::presets {

/// @brief Memory-constrained cache presets.
namespace memory {

/// @brief Alias for caches constrained by total memory usage.
/// @tparam Key The cache key type.
/// @tparam Value The cache value type.
/// @tparam InsertionPolicy The insertion policy template.
/// @tparam EvictionPolicy The eviction policy template.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A callable type returning a hash of a key.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         template<class, class, class> class InsertionPolicy,
         template<class, class, class> class EvictionPolicy,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using MemoryConstrainedCache = Cache<Key, Value, InsertionPolicy, EvictionPolicy, policy::ConstraintMemory, MeasureValue, MeasureKey, KeyHash, Locking>;

/// @brief Least-Recently-Used Cache.
/// @details Uses a linked list to order items from hottest (most recently accessed) to coldest (least recently accessed).
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using LRUCache = MemoryConstrainedCache<Key, Value, policy::InsertionAlways, policy::EvictionLRU, MeasureValue, MeasureKey, KeyHash, Locking>;

/// @brief TinyLFU Cache.
/// @details Uses a combination of frequency sketches to gather a decent estimate of the access frequency of most keys.
///          Uses this estimate to decide which item should be held in cache over another.
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using TinyLFUCache = MemoryConstrainedCache<Key, Value, policy::InsertionTinyLFU, policy::EvictionSegmentedLRU, MeasureValue, MeasureKey, KeyHash, Locking>;

/// @brief Custom-Cost Cache.
/// @details The use of this cache should be favored in scenarios where the cost of a cache miss varies greatly from one item to the next.
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam Cost A functor taking a `const Item<Key, Value>&` returning the cost to load this item in cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         typename Cost,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using CustomCostCache = MemoryConstrainedCache<Key,
                                               Value,
                                               policy::InsertionAlways,
                                               policy::bind<policy::EvictionGDSF, Cost>::template ttype,
                                               MeasureValue,
                                               MeasureKey,
                                               KeyHash,
                                               Locking>;

}  // namespace memory

/// @brief Count-constrained cache presets.
namespace count {

/// @brief Alias for caches constrained by item count.
/// @tparam Key The cache key type.
/// @tparam Value The cache value type.
/// @tparam InsertionPolicy The insertion policy template.
/// @tparam EvictionPolicy The eviction policy template.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A callable type returning a hash of a key.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         template<class, class, class> class InsertionPolicy,
         template<class, class, class> class EvictionPolicy,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using CountConstrainedCache = Cache<Key, Value, InsertionPolicy, EvictionPolicy, policy::ConstraintCount, MeasureValue, MeasureKey, KeyHash, Locking>;

/// @brief Least-Recently-Used Cache.
/// @details Uses a linked list to order items from hottest (most recently accessed) to coldest (least recently accessed).
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using LRUCache = CountConstrainedCache<Key, Value, policy::InsertionAlways, policy::EvictionLRU, MeasureValue, MeasureKey, KeyHash, Locking>;

/// @brief TinyLFU Cache.
/// @details Uses a combination of frequency sketches to gather a decent estimate of the access frequency of most keys.
///          Uses this estimate to decide which item should be held in cache over another.
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using TinyLFUCache = CountConstrainedCache<Key, Value, policy::InsertionTinyLFU, policy::EvictionSegmentedLRU, MeasureValue, MeasureKey, KeyHash, Locking>;

/// @brief Custom-Cost Cache.
/// @details The use of this cache should be favored in scenarios where the cost of a cache miss varies greatly from one item to the next.
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam Cost A functor taking a `const Item<Key, Value>&` returning the cost to load this item in cache.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         typename Cost,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
using CustomCostCache = CountConstrainedCache<Key,
                                              Value,
                                              policy::InsertionAlways,
                                              policy::bind<policy::EvictionGDSF, Cost>::template ttype,
                                              MeasureValue,
                                              MeasureKey,
                                              KeyHash,
                                              Locking>;
}  // namespace count

}  // namespace cachemere::presets
