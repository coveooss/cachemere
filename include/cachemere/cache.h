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

#include "item.h"
#include "measurement.h"

/// @brief Root namespace
namespace cachemere {

/// @brief Thread-safe memory-restricted cache.
/// @details This class keeps the inserted items alive, and it handles
///          the bulk of the Insert/Evict logic while respecting the size constraints.
///
///          Some logic is delegated to the insertion and eviction policies. For details, see the \ref index "Main Page".
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

    /// @brief Check whether a given key is stored in the cache.
    /// @param key The key whose presence to test.
    /// @return Whether the key is in cache.
    bool contains(const Key& key) const;

    /// @brief Find a given key in cache returning the associated value when it exists.
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

    /// @brief Remove a key and its value from the cache.
    /// @details If the key is not present in cache, no operation is taken.
    /// @param key The key to remove from the cache.
    /// @return Whether the key was present in cache.
    bool remove(const Key& key);

    /// @brief Retain all objects matching a predicate.
    /// @details Removes all items for which `predicate_fn` returns false.
    /// @param predicate_fn The predicate function.
    /// @tparam P The type of the predicate function.
    ///           The predicate should have the signature `bool fn(const Key& key, const Value& value)`.
    template<typename P> void retain(P predicate_fn);

    /// @brief Get the number of items currently stored in the cache.
    /// @warning This method acquires a mutual exclusion lock to secure the item count.
    ///          Calling this excessively while the cache is under contention will be detrimental to performance.
    /// @return How many items are in cache.
    [[nodiscard]] size_t number_of_items() const;

    /// @brief Get the amount of memory currently being used by cache items.
    /// @details This method returns the amount of memory used by the cache. For every item,
    ///          the cache stores the key and its value, along with an additional copy of the key.
    ///          This brings the total memory used by an item to `2 * MeasureKey(key) + MeasureValue(value)`.
    /// @return The current memory usage, in bytes.
    [[nodiscard]] size_t size() const;

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

    /// @brief Compute and return the running hit rate of the cache.
    /// @details The hit rate is computed using a sliding window determined by the sliding window
    ///          size passed to the constructor.
    /// @return The hit rate, as a fraction.
    [[nodiscard]] double hit_rate() const;

    /// @brief Compute and return the running byte hit rate of the cache, in bytes.
    /// @details The byte hit rate represents the average amount of data saved by cache accesses.
    ///          This is a very useful metric in applications where item load times scale
    ///          linearly with item size, for instance in a web server.
    /// @return The byte hit rate, in bytes.
    [[nodiscard]] double byte_hit_rate() const;

private:
    using CacheItem = Item<Key, Value>;
    using DataMap   = std::map<Key, CacheItem>;
    using DataMapIt = typename DataMap::iterator;

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

    mutable MeanAccumulator m_hit_rate_acc;
    mutable MeanAccumulator m_byte_hit_rate_acc;

    bool   compare_evict(const Key& candidate_key, size_t candidate_size);
    size_t free_amount(size_t amount_to_free);
    void   remove(DataMapIt it);

    void on_insert(const CacheItem& item) const;
    void on_update(const CacheItem& item) const;
    void on_cache_hit(const CacheItem& item) const;
    void on_cache_miss(const Key& key) const;
    void on_evict(const Key& key) const;
};

}  // namespace cachemere

#include "cache.hpp"

#endif
