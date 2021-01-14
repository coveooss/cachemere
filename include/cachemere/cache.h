#ifndef CACHEMERE_CACHE_H
#define CACHEMERE_CACHE_H

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/hana.hpp>

#include "detail/item.h"
#include "measurement.h"

namespace cachemere {

/// @brief Generic memory-restricted cache.
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam InsertionPolicy A template parameterized by `Key` and `Value` implementing the insertion policy interface.
/// @tparam EvictionPolicy A template parameterized by `Key` and `Value` implementing the eviction policy interface.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
template<typename Key,
         typename Value,
         template<class, class>
         class InsertionPolicy,
         template<class, class>
         class EvictionPolicy,
         typename MeasureValue = measurement::Size<Value>,
         typename MeasureKey   = measurement::Size<Key>>
class Cache
{
public:
    using MyInsertionPolicy = InsertionPolicy<Key, Value>;
    using MyEvictionPolicy  = EvictionPolicy<Key, Value>;

    /// @brief Simple constructor.
    /// @param maximum_size The maximum amount memory to be used by the cache (in bytes).
    /// @param statistics_window_size The length of the cache history to be kept for statistics.
    Cache(size_t maximum_size, uint32_t statistics_window_size = 1000);

    /// @brief Check if a given key is stored in the cache.
    /// @param key The key whose presence needs to be tested.
    /// @return Whether the key is in cache.
    bool contains(const Key& key) const;

    /// @brief Find a given key in cache, optionally returning the associated value.
    /// @param key The key to lookup.
    /// @return The value if `key` is in cache, `std::nullopt` otherwise.
    std::optional<Value> find(const Key& key) const;

    /// @brief Insert a key/value pair in the cache.
    /// @details If the key is new, the key/value pair will be inserted.
    ///          If the key already exists, the provided value will overwrite the previous one.
    /// @param key The key to associate with the value.
    /// @param value The value to store.
    /// @return Whether the item was inserted in cache.
    bool insert(const Key& key, const Value& value);

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t memory_used() const;
    [[nodiscard]] size_t number_of_items() const;

    /// @brief Get the maximum amount of memory that can be used by the cache.
    /// @details Once this size is reached, any future successful insertions will trigger
    ///          evictions of one or more items.
    /// @return The maximum size, in bytes.
    [[nodiscard]] size_t maximum_size() const;

    /// @brief Set the maximum amount of memory that can be used by the cache.
    /// @details If the new maximum size is inferior to the maximum, the cache will resize itself and evict items
    ///          until the new maximum is respected.
    /// @param max_size The new maximum size, in bytes.
    void set_maximum_size(size_t max_size);

    /// @brief Get a reference to the insertion policy used by the cache.
    [[nodiscard]] MyInsertionPolicy& insertion_policy();

    /// @brief Get a reference to the eviction policy used by the cache.
    [[nodiscard]] MyEvictionPolicy& eviction_policy();

    /// @brief Compute and return the running hit ratio of the cache.
    /// @details The hit ratio is computed using a sliding window determined by the sliding window
    ///          size passed to the constructor.
    /// @return The hit ratio, as a fraction.
    [[nodiscard]] double hit_ratio() const;

    /// @brief Compute and return the running byte hit ratio of the cache, in bytes.
    /// @details The byte hit ratio represents the average amount of data saved by cache accesses.
    ///          This is a very useful metric in applications where item load times scale
    ///          linearly with item size, for instance in a web server.
    /// @return The byte hit ratio, in bytes.
    [[nodiscard]] double byte_hit_ratio() const;

private:
    using CacheItem = detail::Item<Key, Value>;
    using DataMap   = std::map<Key, CacheItem>;

    using MyInsertionPolicySP = std::unique_ptr<MyInsertionPolicy>;
    using MyEvictionPolicySP  = std::unique_ptr<MyEvictionPolicy>;

    using RollingMeanTag        = boost::accumulators::tag::rolling_mean;
    using RollingMeanStatistics = boost::accumulators::stats<RollingMeanTag>;
    using MeanAccumulator       = boost::accumulators::accumulator_set<uint32_t, RollingMeanStatistics>;

    std::atomic<size_t> m_current_size;
    std::atomic<size_t> m_maximum_size;

    MyInsertionPolicySP m_insertion_policy;
    MyEvictionPolicySP  m_eviction_policy;

    MeasureKey   m_measure_key;
    MeasureValue m_measure_value;

    mutable std::mutex m_mutex;
    DataMap            m_data;

    mutable MeanAccumulator m_hit_ratio_acc;
    mutable MeanAccumulator m_byte_hit_ratio_acc;

    bool   compare_evict(const Key& candidate_key, size_t candidate_size);
    size_t free_amount(size_t amount_to_free);

    void on_insert(const CacheItem& item) const;
    void on_update(const CacheItem& item) const;
    void on_cache_hit(const CacheItem& item) const;
    void on_cache_miss(const Key& key) const;
    void on_evict(const Key& key) const;
};

}  // namespace cachemere

#include "cache.hpp"

#endif
