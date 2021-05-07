#ifndef CACHEMERE_CACHE_H
#define CACHEMERE_CACHE_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

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
/// @tparam ThreadSafe Whether to enable locking. When true, all cache operations will be protected by a lock. `true` by default.
template<typename Key,
         typename Value,
         template<class, class>
         class InsertionPolicy,
         template<class, class>
         class EvictionPolicy,
         typename MeasureValue = measurement::Size<Value>,
         typename MeasureKey   = measurement::Size<Key>,
         bool ThreadSafe       = true>
class Cache
{
public:
    using MyInsertionPolicy = InsertionPolicy<Key, Value>;
    using MyEvictionPolicy  = EvictionPolicy<Key, Value>;
    using CacheType         = Cache<Key, Value, InsertionPolicy, EvictionPolicy, MeasureValue, MeasureKey, ThreadSafe>;

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

    /// @brief Copy the cache contents in the provided container.
    /// @details The container should conform to either of the STL's interfaces for associative
    ///          containers or for sequence containers.
    ///          Uses `emplace_back` for sequence containers, and `emplace` for associative containers.
    ///          If the provided container has `size()` and `reserve()` methods, `collect_into` will reserve
    ///          the appropriate amount of space in the container before inserting.
    /// @param container The container in which to insert the items.
    template<typename C> void collect_into(C& container) const;

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

    /// @brief Clears the cache contents.
    void clear();

    /// @brief Retain all objects matching a predicate.
    /// @details Removes all items for which `predicate_fn` returns false.
    /// @param predicate_fn The predicate function.
    /// @tparam P The type of the predicate function.
    ///           The predicate should have the signature `bool fn(const Key& key, const Value& value)`.
    template<typename P> void retain(P predicate_fn);

    /// @brief Swaps the current cache with another cache of the same type.
    /// @param other The cache to swap this instance with.
    void swap(CacheType& other);

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

protected:
    std::unique_lock<std::recursive_mutex> lock() const;

private:
    using CacheItem = Item<Key, Value>;
    using DataMap   = std::map<Key, CacheItem>;
    using DataMapIt = typename DataMap::iterator;

    using MyInsertionPolicySP = std::unique_ptr<MyInsertionPolicy>;
    using MyEvictionPolicySP  = std::unique_ptr<MyEvictionPolicy>;

    using RollingMeanTag        = boost::accumulators::tag::rolling_mean;
    using RollingMeanStatistics = boost::accumulators::stats<RollingMeanTag>;
    using MeanAccumulator       = boost::accumulators::accumulator_set<uint32_t, RollingMeanStatistics>;

    size_t   m_current_size;
    size_t   m_maximum_size;
    uint32_t m_statistics_window_size;

    MyInsertionPolicySP m_insertion_policy;
    MyEvictionPolicySP  m_eviction_policy;

    MeasureKey   m_measure_key;
    MeasureValue m_measure_value;

    mutable std::recursive_mutex m_mutex;
    DataMap                      m_data;

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

template<typename K, typename V, template<class, class> class I, template<class, class> class E, typename SV, typename SK, bool TS>
void swap(Cache<K, V, I, E, SV, SK, TS>& lhs, Cache<K, V, I, E, SV, SK, TS>& rhs);

}  // namespace cachemere

#include "cache.hpp"

#endif
