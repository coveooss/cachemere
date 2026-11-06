#ifndef CACHEMERE_CONSTRAINT_TIME_TO_LIVE_H
#define CACHEMERE_CONSTRAINT_TIME_TO_LIVE_H

#include <cassert>
#include <chrono>
#include <functional>

#include <absl/container/flat_hash_map.h>

#include "cachemere/item.h"

namespace cachemere::policy {

/// @brief Time-to-live constraint.
/// @details An item expires once the time-to-live has passed since it was last inserted, updated, or hit.
///          The cache removes expired items on each insertion and in `Cache::remove_expired()`.
///          This constraint doesn't limit the size of the cache.
/// @tparam Key The type of the keys used to identify items in the cache.
/// @tparam KeyHash The type of the hasher used to hash item keys.
/// @tparam Value The type of the values stored in the cache.
/// @tparam Clock The clock used to measure time.
template<typename Key, typename KeyHash, typename Value, typename Clock> class BasicConstraintTimeToLive
{
    using CacheItem = Item<Value>;
    using Duration  = typename Clock::duration;
    using TimePoint = typename Clock::time_point;
    using KeyRef    = std::reference_wrapper<const Key>;

public:
    /// @brief Constructor.
    /// @param time_to_live How long items can stay in cache without being used.
    explicit BasicConstraintTimeToLive(Duration time_to_live);

    /// @brief Clears the policy.
    void clear();

    /// @brief Always `true`: the time-to-live doesn't limit the size of the cache.
    [[nodiscard]] bool can_add(const Key& key, const CacheItem& item);

    /// @brief Always `true`: the time-to-live doesn't limit the size of the cache.
    [[nodiscard]] bool can_replace(const Key& key, const CacheItem& old_item, const CacheItem& new_item);

    /// @brief Always `true`: the time-to-live doesn't limit the size of the cache.
    [[nodiscard]] bool is_satisfied();

    /// @brief Check whether an item expired.
    /// @param key The key of an item in cache.
    /// @return Whether the time-to-live has passed since the item was last used.
    [[nodiscard]] bool is_expired(const Key& key) const;

    /// @brief Insertion event handler. Starts the time-to-live of the item.
    void on_insert(const Key& key, const CacheItem& item);

    /// @brief Update event handler. Restarts the time-to-live of the item.
    void on_update(const Key& key, const CacheItem& old_item, const CacheItem& new_item);

    /// @brief Cache hit event handler. Restarts the time-to-live of the item.
    void on_cache_hit(const Key& key, const CacheItem& item);

    /// @brief Eviction event handler. Stops tracking the item.
    void on_evict(const Key& key, const CacheItem& item);

private:
    Duration                                                            m_time_to_live;
    absl::flat_hash_map<KeyRef, TimePoint, KeyHash, std::equal_to<Key>> m_last_used;
};

/// @brief Time-to-live constraint measuring time with `std::chrono::steady_clock`.
template<typename Key, typename KeyHash, typename Value> using ConstraintTimeToLive = BasicConstraintTimeToLive<Key, KeyHash, Value, std::chrono::steady_clock>;

}  // namespace cachemere::policy

#include "constraint_time_to_live.hpp"

#endif
