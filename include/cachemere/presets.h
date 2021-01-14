#ifndef CACHEMERE_PRESETS_H
#define CACHEMERE_PRESETS_H

#include "cache.h"
#include "measurement.h"

#include "policy/eviction_lru.h"
#include "policy/eviction_segmented_lru.h"
#include "policy/insertion_always.h"
#include "policy/insertion_tinylfu.h"

namespace cachemere::presets {

// Standard Least-Recently-Used Cache.
template<typename Key, typename Value, typename MeasureValue = measurement::Size<Value>, typename MeasureKey = measurement::Size<Key>>
using LRUCache = Cache<Key, Value, policy::InsertionAlways, policy::EvictionLRU, MeasureValue, MeasureKey>;

// TLFU Cache
template<typename Key, typename Value, typename MeasureValue = measurement::Size<Value>, typename MeasureKey = measurement::Size<Key>>
using TinyLFUCache = Cache<Key, Value, policy::InsertionTinyLFU, policy::EvictionSegmentedLRU, MeasureValue, MeasureKey>;

}  // namespace cachemere::presets

#endif
