#ifndef CACHEMERE_CACHE_H
#define CACHEMERE_CACHE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <absl/container/node_hash_map.h>
#include <absl/hash/hash.h>

#ifdef _WIN32
#    pragma warning(push)
#    pragma warning(disable : 4244)
#    pragma warning(disable : 4018)
#endif

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/hana.hpp>

#ifdef _WIN32
#    pragma warning(pop)
#endif

#include "item.h"
#include "measurement.h"
#include "detail/traits.h"

/// @brief Root namespace
namespace cachemere {

/// @brief Thread-safe memory-restricted cache.
/// @details This class keeps the inserted items alive, and it handles
///          the bulk of the Insert/Evict logic while respecting the size constraints.
///
///          Some logic is delegated to the insertion and eviction policies. For details, see the \ref index "Main Page".
/// @tparam Key The type of the key used for retrieving items.
/// @tparam Value The type of the items stored in the cache.
/// @tparam InsertionPolicy A template parameterized by `Key`, `KeyHash`, and `Value` implementing the insertion policy interface.
/// @tparam EvictionPolicy A template parameterized by `Key` `KeyHash`, and `Value` implementing the eviction policy interface.
/// @tparam ConstraintPolicy A template parameterized by `Key` `KeyHash`, and `Value` implementing the constraint policy interface.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam ThreadSafe Whether to enable locking. When true, all cache operations will be protected by a lock. `true` by default.
template<typename Key,
         typename Value,
         template<class, class, class>
         class InsertionPolicy,
         template<class, class, class>
         class EvictionPolicy,
         template<class, class, class>
         class ConstraintPolicy,
         typename MeasureValue = measurement::Size<Value>,
         typename MeasureKey   = measurement::Size<Key>,
         typename KeyHash      = absl::Hash<Key>,
         bool ThreadSafe       = true>
class Cache
{
public:
    using MyInsertionPolicy  = InsertionPolicy<Key, KeyHash, Value>;
    using MyEvictionPolicy   = EvictionPolicy<Key, KeyHash, Value>;
    using MyConstraintPolicy = ConstraintPolicy<Key, KeyHash, Value>;
    using CacheType          = Cache<Key, Value, InsertionPolicy, EvictionPolicy, ConstraintPolicy, MeasureValue, MeasureKey, KeyHash, ThreadSafe>;
    using LockGuard          = std::unique_lock<std::recursive_mutex>;

    /// @brief Simple constructor.
    /// @param args Arguments to forward to the constraint policy constructor.
    template<typename... Args> Cache(Args... args);

    /// @brief Constructor to initialize the cache with a set of items.
    /// @details Will insert items in order in the cache as long as the constraint is satisfied.
    /// @param collection The collection to use to initialize the cache - must be a collection of pairs.
    /// @param args Tuple of arguments to forward to the constraint policy constructor.
    /// @warning Items that are imported from the collection are moved out of the container and left
    ///          in an unspecified state. The container itself can be reused after clearing it.
    template<typename C, typename... Args> Cache(C& collection, std::tuple<Args...> args);

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
    bool insert(Key key, Value value);

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

    /// @brief Apply a function to all objects in cache.
    /// @param unary_function The function to be applied to all items in cache.
    ///                       The function should have the signature `void fn(const Key& key, const Value& value)`.
    template<typename F> void for_each(F unary_function);

    /// @brief Swaps the current cache with another cache of the same type.
    /// @details Calls `std::terminate` if the cache runs in thread-safe mode and an exception is thrown while locking its mutex.
    /// @param other The cache to swap this instance with.
    void swap(CacheType& other) noexcept;

    /// @brief Get the number of items currently stored in the cache.
    /// @warning This method acquires a mutual exclusion lock to secure the item count.
    ///          Calling this excessively while the cache is under contention will be detrimental to performance.
    /// @return How many items are in cache.
    [[nodiscard]] size_t number_of_items() const;

    /// @brief Update the cache constraint.
    /// @details Forwards the update to the constraint and evicts items from the cache until the
    ///          constraint is satisfied.
    /// @param args Arguments to forward to the `Constraint::update()` .
    template<typename... Args> void update_constraint(Args... args);

    /// @brief Get a reference to the insertion policy used by the cache.
    [[nodiscard]] MyInsertionPolicy& insertion_policy();

    /// @brief Get a const reference to the insertion policy used by the cache.
    [[nodiscard]] const MyInsertionPolicy& insertion_policy() const;

    /// @brief Get a reference to the eviction policy used by the cache.
    [[nodiscard]] MyEvictionPolicy& eviction_policy();

    /// @brief Get a const reference to the eviction policy used by the cache.
    [[nodiscard]] const MyEvictionPolicy& eviction_policy() const;

    /// @brief Get a reference to the constraint policy used by the cache.
    [[nodiscard]] MyConstraintPolicy& constraint_policy();

    /// @brief Get a const reference to the constraint policy used by the cache.
    [[nodiscard]] const MyConstraintPolicy& constraint_policy() const;

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

    /// @brief Get the size of the sliding window used for computing statistics.
    /// @return The size of the statistics sliding window.
    [[nodiscard]] uint32_t statistics_window_size() const;

    /// @brief Set the size of the sliding window used for computing statistics.
    /// @warning This will reset the access log, so cache accesses made prior to calling this will not be
    ///          counted in the statistics.
    /// @param window_size The desired statistics window size.
    void statistics_window_size(uint32_t window_size);

protected:
    LockGuard                       lock() const;
    LockGuard                       lock(std::defer_lock_t defer_lock_tag) const;
    std::pair<LockGuard, LockGuard> lock_pair(CacheType& other) const;

    template<typename C> void import(C& collection);

private:
    using CacheItem = Item<Value>;

    using DataMap = absl::node_hash_map<Key, CacheItem, KeyHash>;

    using DataMapIt = typename DataMap::iterator;

    using MyInsertionPolicySP  = std::unique_ptr<MyInsertionPolicy>;
    using MyEvictionPolicySP   = std::unique_ptr<MyEvictionPolicy>;
    using MyConstraintPolicySP = std::unique_ptr<MyConstraintPolicy>;

    using RollingMeanTag        = boost::accumulators::tag::rolling_mean;
    using RollingMeanStatistics = boost::accumulators::stats<RollingMeanTag>;
    using MeanAccumulator       = boost::accumulators::accumulator_set<uint32_t, RollingMeanStatistics>;

    uint32_t m_statistics_window_size = 1000;

    MyInsertionPolicySP  m_insertion_policy;
    MyEvictionPolicySP   m_eviction_policy;
    MyConstraintPolicySP m_constraint_policy;

    MeasureKey   m_measure_key;
    MeasureValue m_measure_value;

    mutable std::recursive_mutex m_mutex;
    DataMap                      m_data;

    mutable MeanAccumulator m_hit_rate_acc;
    mutable MeanAccumulator m_byte_hit_rate_acc;

    bool check_insert(const Key& candidate_key, const CacheItem& item);
    bool check_replace(const Key& candidate_key, const CacheItem& old_item, const CacheItem& new_item);

    void insert_or_update(Key&& key, CacheItem&& value);
    void remove(DataMapIt it);

    void on_insert(const Key& key, const CacheItem& item) const;
    void on_update(const Key& key, const CacheItem& old_item, const CacheItem& new_item) const;
    void on_cache_hit(const Key& key, const CacheItem& item) const;
    void on_cache_miss(const Key& key) const;
    void on_evict(const Key& key, const CacheItem& item) const;
};

template<class K,
         class V,
         template<class, class, class>
         class I,
         template<class, class, class>
         class E,
         template<class, class, class>
         class C,
         class SV,
         class SK,
         class KH,
         bool TS>
void swap(Cache<K, V, I, E, C, SV, SK, KH, TS>& lhs, Cache<K, V, I, E, C, SV, SK, KH, TS>& rhs) noexcept;

}  // namespace cachemere

#include "cache.hpp"

#endif
