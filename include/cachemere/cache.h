#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
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

#ifdef _WIN32
#    pragma warning(pop)
#endif

#include "detail/locking.h"
#include "detail/transparent_eq.h"
#include "detail/traits.h"
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
/// @tparam InsertionPolicy A template parameterized by `Key`, `KeyHash`, and `Value` implementing the insertion policy interface.
/// @tparam EvictionPolicy A template parameterized by `Key` `KeyHash`, and `Value` implementing the eviction policy interface.
/// @tparam ConstraintPolicy A template parameterized by `Key` `KeyHash`, and `Value` implementing the constraint policy interface.
/// @tparam MeasureValue A functor returning the size of a cache value.
/// @tparam MeasureKey A functor returning the size of a cache key.
/// @tparam KeyHash A default-constructible callable type returning a hash of a key. Defaults to `absl::Hash<Key>`.
/// @tparam Locking The locking strategy used to protect cache operations. Defaults to `LockingStrategy::Mutex`.
template<typename Key,
         typename Value,
         template<class, class, class> class InsertionPolicy,
         template<class, class, class> class EvictionPolicy,
         template<class, class, class> class ConstraintPolicy,
         typename MeasureValue   = measurement::Size<Value>,
         typename MeasureKey     = measurement::Size<Key>,
         typename KeyHash        = absl::Hash<Key>,
         LockingStrategy Locking = LockingStrategy::Mutex>
class Cache
{
public:
    using MyInsertionPolicy  = InsertionPolicy<Key, KeyHash, Value>;
    using MyEvictionPolicy   = EvictionPolicy<Key, KeyHash, Value>;
    using MyConstraintPolicy = ConstraintPolicy<Key, KeyHash, Value>;
    using CacheType          = Cache<Key, Value, InsertionPolicy, EvictionPolicy, ConstraintPolicy, MeasureValue, MeasureKey, KeyHash, Locking>;
    using MutexType          = detail::MutexForT<Locking>;
    using LockGuard          = std::unique_lock<MutexType>;
    struct TryFindResult {
        bool                 lock_acquired = false;
        std::optional<Value> value;
    };
    struct TryInsertResult {
        bool lock_acquired = false;
        bool inserted      = false;
    };

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
    /// @tparam KeyView The type of the key used for retrieving items.
    /// @param key The key whose presence to test.
    /// @return Whether the key is in cache.
    template<typename KeyView> bool contains(const KeyView& key) const;

    /// @brief Find a given key in cache returning the associated value when it exists.
    /// @tparam KeyView The type of the key used for retrieving items.
    /// @param key The key to lookup.
    /// @return The value if `key` is in cache, `std::nullopt` otherwise.
    template<typename KeyView> std::optional<Value> find(const KeyView& key) const;

    /// @brief Try to find a given key in cache without blocking for the cache lock.
    /// @tparam KeyView The type of the key used for retrieving items.
    /// @param key The key to lookup.
    /// @return A result indicating whether the lock was acquired and, if so, whether the key was found.
    template<typename KeyView> TryFindResult try_find(const KeyView& key) const;

    /// @brief Try to find a given key in cache, waiting for up to the provided timeout to acquire the lock.
    /// @tparam KeyView The type of the key used for retrieving items.
    /// @param key The key to lookup.
    /// @param timeout The maximum amount of time to wait for the cache lock.
    /// @return A result indicating whether the lock was acquired and, if so, whether the key was found.
    template<typename KeyView,
             typename Rep,
             typename Period,
             LockingStrategy TimedLocking                                  = Locking,
             std::enable_if_t<detail::HasTimedLockingV<TimedLocking>, int> = 0>
    TryFindResult try_find(const KeyView& key, const std::chrono::duration<Rep, Period>& timeout) const;

    /// @brief Find a given key in cache returning the associated value when it exists, or inserting a new value if it doesn't.
    /// @tparam KeyView The type of the key used for retrieving items.
    /// @tparam Fn A factory function type, with signature `Value fn(const KeyView& key)`, used to generate the value to insert if the key is not in cache.
    /// @param key The key to lookup.
    /// @param fn The factory function to generate the value if the key is not in cache.
    /// @return The value associated with the key.
    template<typename KeyView, detail::traits::FactoryFn<KeyView, Value> Fn> Value find_or_insert(const KeyView& key, Fn&& fn);

    /// @brief Copy the cache contents in the provided container.
    /// @details The container should conform to either of the STL's interfaces for associative
    ///          containers or for sequence containers.
    ///          Uses `emplace_back` for sequence containers, and `emplace` for associative containers.
    ///          If the provided container has `size()` and `reserve()` methods, `collect_into` will reserve
    ///          the appropriate amount of space in the container before inserting.
    /// @param container The container in which to insert the items.
    template<detail::traits::CacheContainer<Key, Value> C> void collect_into(C& container) const;

    /// @brief Insert a key/value pair in the cache.
    /// @details If the key is new, the key/value pair will be inserted.
    ///          If the key already exists, the provided value will overwrite the previous one.
    /// @param key The key to associate with the value.
    /// @param value The value to store.
    /// @return Whether the item was inserted in cache.
    bool insert(Key key, Value value);

    /// @brief Try to insert a key/value pair in the cache without blocking for the cache lock.
    /// @param key The key to associate with the value.
    /// @param value The value to store.
    /// @return A result indicating whether the lock was acquired and, if so, whether the item was inserted.
    TryInsertResult try_insert(Key key, Value value);

    /// @brief Try to insert a key/value pair in the cache, waiting for up to the provided timeout to acquire the lock.
    /// @param key The key to associate with the value.
    /// @param value The value to store.
    /// @param timeout The maximum amount of time to wait for the cache lock.
    /// @return A result indicating whether the lock was acquired and, if so, whether the item was inserted.
    template<typename Rep, typename Period, LockingStrategy TimedLocking = Locking, std::enable_if_t<detail::HasTimedLockingV<TimedLocking>, int> = 0>
    TryInsertResult try_insert(Key key, Value value, const std::chrono::duration<Rep, Period>& timeout);

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
    LockGuard lock() const;
    LockGuard try_lock() const;
    LockGuard lock(std::defer_lock_t defer_lock_tag) const;
    template<typename Rep, typename Period, LockingStrategy TimedLocking = Locking, std::enable_if_t<detail::HasTimedLockingV<TimedLocking>, int> = 0>
    LockGuard                       try_lock(const std::chrono::duration<Rep, Period>& timeout) const;
    std::pair<LockGuard, LockGuard> lock_pair(CacheType& other) const;

    template<typename C> void import(C& collection);

private:
    using CacheItem = Item<Value>;

    using DataMap = absl::node_hash_map<Key, CacheItem, KeyHash, detail::TransparentEq<Key>>;

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

    mutable MutexType m_mutex;
    DataMap           m_data;

    mutable MeanAccumulator m_hit_rate_acc;
    mutable MeanAccumulator m_byte_hit_rate_acc;

    template<typename KeyView> std::optional<Value> find_locked(const KeyView& key) const;
    bool                                            insert_locked(Key key, Value value);
    template<class ConstraintTicket> bool           collect_evictions(const Key& candidate_key, ConstraintTicket& ticket);
    bool                                            check_insert(const Key& candidate_key, const CacheItem& item);
    bool                                            check_replace(const Key& candidate_key, const CacheItem& old_item, const CacheItem& new_item);

    void insert_or_update(Key&& key, CacheItem&& value);
    void remove(DataMapIt it);
    void remove_popped_victim(DataMapIt it);

    void                            on_insert(const Key& key, const CacheItem& item) const;
    void                            on_update(const Key& key, const CacheItem& old_item, const CacheItem& new_item) const;
    void                            on_cache_hit(const Key& key, const CacheItem& item) const;
    template<typename KeyView> void on_cache_miss(const KeyView& key) const;
    void                            on_evict(const Key& key, const CacheItem& item) const;
};

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void swap(Cache<K, V, I, E, C, SV, SK, KH, L>& lhs, Cache<K, V, I, E, C, SV, SK, KH, L>& rhs) noexcept;

template<typename Key,
         typename Value,
         template<class, class, class> class InsertionPolicy,
         template<class, class, class> class EvictionPolicy,
         template<class, class, class> class ConstraintPolicy,
         typename MeasureValue,
         typename MeasureKey,
         typename KeyHash,
         LockingStrategy Locking>
template<typename... Args>
Cache<Key, Value, InsertionPolicy, EvictionPolicy, ConstraintPolicy, MeasureValue, MeasureKey, KeyHash, Locking>::Cache(Args... args)
 : m_insertion_policy(std::make_unique<MyInsertionPolicy>()),
   m_eviction_policy(std::make_unique<MyEvictionPolicy>()),
   m_constraint_policy(std::make_unique<MyConstraintPolicy>(std::forward<Args>(args)...)),
   m_mutex{},
   m_data{},
   m_hit_rate_acc(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size),
   m_byte_hit_rate_acc(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size)
{
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename Coll, typename... Args>
Cache<K, V, I, E, C, SV, SK, KH, L>::Cache(Coll& collection, std::tuple<Args...> args)
 : m_insertion_policy(std::make_unique<MyInsertionPolicy>()),
   m_eviction_policy(std::make_unique<MyEvictionPolicy>()),
   m_constraint_policy(std::move(
       std::apply([](auto&&... params) { return std::make_unique<MyConstraintPolicy>(std::forward<decltype(params)>(params)...); }, std::move(args)))),
   m_mutex{},
   m_data{},
   m_hit_rate_acc(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size),
   m_byte_hit_rate_acc(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size)
{
    import(collection);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename KeyView>
inline bool Cache<K, V, I, E, C, SV, SK, KH, L>::contains(const KeyView& key) const
{
    LockGuard guard(lock());
    return m_data.find(key) != m_data.end();
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename KeyView>
std::optional<V> Cache<K, V, I, E, C, SV, SK, KH, L>::find_locked(const KeyView& key) const
{
    auto key_and_item = m_data.find(key);
    if (key_and_item != m_data.end()) {
        on_cache_hit(key_and_item->first, key_and_item->second);
        return key_and_item->second.m_value;
    }

    on_cache_miss(key);
    return std::nullopt;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename KeyView>
std::optional<V> Cache<K, V, I, E, C, SV, SK, KH, L>::find(const KeyView& key) const
{
    LockGuard guard(lock());
    return find_locked(key);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename KeyView>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::try_find(const KeyView& key) const -> TryFindResult
{
    LockGuard guard(try_lock());
    if (!guard.owns_lock()) {
        return TryFindResult{false, std::nullopt};
    }

    return TryFindResult{true, find_locked(key)};
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename KeyView, typename Rep, typename Period, LockingStrategy TimedLocking, std::enable_if_t<detail::HasTimedLockingV<TimedLocking>, int>>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::try_find(const KeyView& key, const std::chrono::duration<Rep, Period>& timeout) const -> TryFindResult
{
    LockGuard guard(try_lock(timeout));
    if (!guard.owns_lock()) {
        return TryFindResult{false, std::nullopt};
    }

    return TryFindResult{true, find_locked(key)};
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename KeyView, detail::traits::FactoryFn<KeyView, V> Fn>
V Cache<K, V, I, E, C, SV, SK, KH, L>::find_or_insert(const KeyView& key, Fn&& fn)
{
    LockGuard guard(lock());

    if (const auto found = find_locked(key); found.has_value()) {
        return *found;
    }

    V value = fn(key);
    insert_locked(K(key), value);
    return value;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<detail::traits::CacheContainer<K, V> Container>
void Cache<K, V, I, E, C, SV, SK, KH, L>::collect_into(Container& container) const
{
    using namespace detail;

    LockGuard guard(lock());

    // Reserve space if the container has a reserve() method and a size method().
    if constexpr (traits::ReservableContainer<Container>) {
        container.reserve(container.size() + m_data.size());
    }

    // Copy the cache contents to the container.
    for (const auto& [key, cached_item] : m_data) {
        // Use emplace_back if container is a sequence container, or emplace if container is an associative container.
        if constexpr (traits::SequenceContainer<Container, K, V>) {
            container.emplace_back(key, cached_item.m_value);
        } else {
            container.emplace(key, cached_item.m_value);
        }
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
bool Cache<K, V, I, E, C, SV, SK, KH, L>::insert_locked(K key, V value)
{
    const auto key_size   = static_cast<size_t>(m_measure_key(key));
    const auto value_size = static_cast<size_t>(m_measure_value(value));

    CacheItem new_item{key_size, std::move(value), value_size};

    auto it = m_data.find(key);
    if (it != m_data.end()) {
        if (check_replace(key, it->second, new_item)) {
            // We call insert_or_update because we might have evicted the original key to make room for this one.
            insert_or_update(std::move(key), std::move(new_item));
            return true;
        }
    } else {
        if (check_insert(key, new_item)) {
            const auto it_and_ok = m_data.insert_or_assign(std::move(key), std::move(new_item));
            assert(it_and_ok.second);

            on_insert(it_and_ok.first->first, it_and_ok.first->second);
            return true;
        }
    }

    return false;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
bool Cache<K, V, I, E, C, SV, SK, KH, L>::insert(K key, V value)
{
    LockGuard guard(lock());
    return insert_locked(std::move(key), std::move(value));
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::try_insert(K key, V value) -> TryInsertResult
{
    LockGuard guard(try_lock());
    if (!guard.owns_lock()) {
        return TryInsertResult{false, false};
    }

    return TryInsertResult{true, insert_locked(std::move(key), std::move(value))};
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename Rep, typename Period, LockingStrategy TimedLocking, std::enable_if_t<detail::HasTimedLockingV<TimedLocking>, int>>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::try_insert(K key, V value, const std::chrono::duration<Rep, Period>& timeout) -> TryInsertResult
{
    LockGuard guard(try_lock(timeout));
    if (!guard.owns_lock()) {
        return TryInsertResult{false, false};
    }

    return TryInsertResult{true, insert_locked(std::move(key), std::move(value))};
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
bool Cache<K, V, I, E, C, SV, SK, KH, L>::remove(const K& key)
{
    LockGuard guard(lock());

    auto key_and_item = m_data.find(key);
    if (key_and_item != m_data.end()) {
        remove(key_and_item);
        return true;
    }
    return false;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::clear()
{
    LockGuard guard(lock());

    m_data.clear();

    m_hit_rate_acc      = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);
    m_byte_hit_rate_acc = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);

    m_insertion_policy->clear();
    m_eviction_policy->clear();
    m_constraint_policy->clear();
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<class P>
void Cache<K, V, I, E, C, SV, SK, KH, L>::retain(P predicate_fn)
{
    LockGuard guard(lock());

    for (auto it = m_data.begin(); it != m_data.end();) {
        const CacheItem& item = it->second;

        if (!predicate_fn(it->first, item.m_value)) {
            remove(it++);
        } else {
            ++it;
        }
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<class F>
void Cache<K, V, I, E, C, SV, SK, KH, L>::for_each(F unary_function)
{
    LockGuard guard(lock());
    for (const auto& [key, value] : m_data) {
        unary_function(key, value.m_value);
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::swap(CacheType& other) noexcept
{
    try {
        // Acquire both cache locks.
        std::pair<LockGuard, LockGuard> guards{lock_pair(other)};

        using std::swap;

        swap(m_statistics_window_size, other.m_statistics_window_size);

        swap(m_insertion_policy, other.m_insertion_policy);
        swap(m_eviction_policy, other.m_eviction_policy);
        swap(m_constraint_policy, other.m_constraint_policy);

        swap(m_data, other.m_data);

        swap(m_hit_rate_acc, other.m_hit_rate_acc);
        swap(m_byte_hit_rate_acc, other.m_byte_hit_rate_acc);
    } catch (const std::system_error& e) {
        // The only exception that can sensibly be thrown in the above block is a `system_error` when acquiring the mutexes of both caches (if the caches
        // are running in thread-safe mode).
        //
        // According to the [reference for std mutexes](https://en.cppreference.com/w/cpp/named_req/Mutex), this exception can be thrown for one of two
        // reasons:
        //   - If the calling thread does not have the required privileges.
        //   - If the implementation determines that acquiring the lock would lead to a deadlock.
        //
        // Since both those errors are unrecoverable, we'll validate that the error code is one of the two we expect, and we'll terminate.
        assert(e.code() == std::errc::operation_not_permitted || e.code() == std::errc::resource_deadlock_would_occur);
        std::terminate();
    } catch (...) {
        // Even though this is technically not possible in the current state of the code, we'll catch any leftover exception to satisfy clang-tidy.
        std::terminate();
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline size_t Cache<K, V, I, E, C, SV, SK, KH, L>::number_of_items() const
{
    LockGuard guard(lock());
    return m_data.size();
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename... Args>
void Cache<K, V, I, E, C, SV, SK, KH, L>::update_constraint(Args... args)
{
    LockGuard guard(lock());
    m_constraint_policy->update(std::forward<Args>(args)...);

    while (!m_constraint_policy->is_satisfied()) {
        auto eviction_ticket = m_eviction_policy->pop_victim();
        auto key_and_item    = m_data.find(eviction_ticket.key());

        // If this trips, the eviction policy tried to evict an item not in cache: the eviction policy and the cache are out of sync.
        assert(key_and_item != m_data.end());

        remove_popped_victim(key_and_item);
    }

    assert(m_constraint_policy->is_satisfied());
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline auto Cache<K, V, I, E, C, SV, SK, KH, L>::insertion_policy() -> MyInsertionPolicy&
{
    return *m_insertion_policy;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline auto Cache<K, V, I, E, C, SV, SK, KH, L>::insertion_policy() const -> const MyInsertionPolicy&
{
    return *m_insertion_policy;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline auto Cache<K, V, I, E, C, SV, SK, KH, L>::eviction_policy() -> MyEvictionPolicy&
{
    return *m_eviction_policy;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline auto Cache<K, V, I, E, C, SV, SK, KH, L>::eviction_policy() const -> const MyEvictionPolicy&
{
    return *m_eviction_policy;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline auto Cache<K, V, I, E, C, SV, SK, KH, L>::constraint_policy() -> MyConstraintPolicy&
{
    return *m_constraint_policy;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline auto Cache<K, V, I, E, C, SV, SK, KH, L>::constraint_policy() const -> const MyConstraintPolicy&
{
    return *m_constraint_policy;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline double Cache<K, V, I, E, C, SV, SK, KH, L>::hit_rate() const
{
    return boost::accumulators::rolling_mean(m_hit_rate_acc);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline double Cache<K, V, I, E, C, SV, SK, KH, L>::byte_hit_rate() const
{
    return boost::accumulators::rolling_mean(m_byte_hit_rate_acc);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline uint32_t Cache<K, V, I, E, C, SV, SK, KH, L>::statistics_window_size() const
{
    return m_statistics_window_size;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
inline void Cache<K, V, I, E, C, SV, SK, KH, L>::statistics_window_size(uint32_t window_size)
{
    m_statistics_window_size = window_size;

    m_hit_rate_acc      = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);
    m_byte_hit_rate_acc = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::lock() const -> LockGuard
{
    LockGuard guard{m_mutex};
    return guard;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::try_lock() const -> LockGuard
{
    LockGuard guard{m_mutex, std::try_to_lock};
    return guard;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::lock([[maybe_unused]] std::defer_lock_t defer_lock_tag) const -> LockGuard
{
    LockGuard guard{m_mutex, defer_lock_tag};
    return guard;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<typename Rep, typename Period, LockingStrategy TimedLocking, std::enable_if_t<detail::HasTimedLockingV<TimedLocking>, int>>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::try_lock(const std::chrono::duration<Rep, Period>& timeout) const -> LockGuard
{
    LockGuard                   guard{m_mutex, std::defer_lock};
    [[maybe_unused]] const bool locked = guard.try_lock_for(timeout);
    return guard;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
auto Cache<K, V, I, E, C, SV, SK, KH, L>::lock_pair(CacheType& other) const -> std::pair<LockGuard, LockGuard>
{
    LockGuard my_guard    = lock(std::defer_lock);
    LockGuard other_guard = other.lock(std::defer_lock);

    std::lock(my_guard, other_guard);

    return std::make_pair<LockGuard, LockGuard>(std::move(my_guard), std::move(other_guard));
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<class Coll>
void Cache<K, V, I, E, C, SV, SK, KH, L>::import(Coll& collection)
{
    LockGuard guard(lock());

    for (auto& [key, value] : collection) {
        const auto key_size   = static_cast<size_t>(m_measure_key(key));
        const auto value_size = static_cast<size_t>(m_measure_value(value));
        CacheItem  item{key_size, std::move(value), value_size};

        if (!m_constraint_policy->prepare_insert(key, item).is_satisfied()) {
            return;
        }

        insert_or_update(std::move(key), std::move(item));
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<class ConstraintTicket>
bool Cache<K, V, I, E, C, SV, SK, KH, L>::collect_evictions(const K& candidate_key, ConstraintTicket& ticket)
{
    std::vector<std::pair<typename MyEvictionPolicy::Ticket, DataMapIt>> keys_to_evict;

    while (!ticket.is_satisfied()) {
        auto eviction_ticket = m_eviction_policy->pop_victim();
        auto key_and_item    = m_data.find(eviction_ticket.key());

        // If this trips, the eviction policy recommended the eviction of a key not in cache: the eviction policy and the
        // cache are out of sync.
        assert(key_and_item != m_data.end());

        if (!m_insertion_policy->should_replace(key_and_item->first, candidate_key)) {
            m_eviction_policy->rollback(std::move(eviction_ticket));
            for (auto it = keys_to_evict.rbegin(); it != keys_to_evict.rend(); ++it) {
                m_eviction_policy->rollback(std::move(it->first));
            }
            return false;
        }

        ticket.register_eviction(key_and_item->first, key_and_item->second);
        keys_to_evict.emplace_back(std::move(eviction_ticket), key_and_item);
    }

    for (auto& [_, key_and_item] : keys_to_evict) {
        remove_popped_victim(key_and_item);
    }

    return true;
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
bool Cache<K, V, I, E, C, SV, SK, KH, L>::check_insert(const K& key, const CacheItem& item)
{
    auto insertion_ticket = m_constraint_policy->prepare_insert(key, item);

    if (insertion_ticket.is_satisfied()) {
        // The constraint is already satisfied, so we just check the insertion policy.
        return m_insertion_policy->should_add(key);
    }

    if (!insertion_ticket.is_satisfiable()) {
        return false;  // item can't be inserted even if we evict everything.
    }

    return collect_evictions(key, insertion_ticket);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
bool Cache<K, V, I, E, C, SV, SK, KH, L>::check_replace(const K& key, const CacheItem& old_item, const CacheItem& new_item)
{
    auto replacement_ticket = m_constraint_policy->prepare_replace(key, old_item, new_item);

    if (replacement_ticket.is_satisfied()) {
        return true;
    }

    if (!replacement_ticket.is_satisfiable()) {
        return false;
    }

    return collect_evictions(key, replacement_ticket);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::insert_or_update(K&& key, CacheItem&& item)
{
    auto key_and_item = m_data.find(key);
    if (key_and_item != m_data.end()) {
        using std::swap;
        swap(key_and_item->second, item);
        on_update(key_and_item->first, item, key_and_item->second);
    } else {
        // Insert.
        const auto it_and_ok = m_data.insert_or_assign(std::move(key), std::move(item));
        assert(it_and_ok.second);
        on_insert(it_and_ok.first->first, it_and_ok.first->second);
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::remove(DataMapIt it)
{
    on_evict(it->first, it->second);
    m_data.erase(it);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::remove_popped_victim(DataMapIt it)
{
    if constexpr (detail::traits::event::HasOnEvict<K, KH, V, I>) {
        m_insertion_policy->on_evict(it->first, it->second);
    }

    if constexpr (detail::traits::event::HasOnEvict<K, KH, V, C>) {
        m_constraint_policy->on_evict(it->first, it->second);
    }

    m_data.erase(it);
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::on_insert(const K& key, const CacheItem& item) const
{
    // Call event handler iif the method is defined in the policy.
    if constexpr (detail::traits::event::HasOnInsert<K, KH, V, I>) {
        m_insertion_policy->on_insert(key, item);
    }

    if constexpr (detail::traits::event::HasOnInsert<K, KH, V, E>) {
        m_eviction_policy->on_insert(key, item);
    }

    if constexpr (detail::traits::event::HasOnInsert<K, KH, V, C>) {
        m_constraint_policy->on_insert(key, item);
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::on_update(const K& key, const CacheItem& old_item, const CacheItem& new_item) const
{
    // Call event handler iif the method is defined in the policy.
    if constexpr (detail::traits::event::HasOnUpdate<K, KH, V, I>) {
        m_insertion_policy->on_update(key, old_item, new_item);
    }

    if constexpr (detail::traits::event::HasOnUpdate<K, KH, V, E>) {
        m_eviction_policy->on_update(key, old_item, new_item);
    }

    if constexpr (detail::traits::event::HasOnUpdate<K, KH, V, C>) {
        m_constraint_policy->on_update(key, old_item, new_item);
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::on_cache_hit(const K& key, const CacheItem& item) const
{
    // Update the cache hit rate accumulators.
    m_hit_rate_acc(1);
    m_byte_hit_rate_acc(static_cast<uint32_t>(item.m_value_size));

    // Call event handler iif the method is defined in the policy.
    if constexpr (detail::traits::event::HasOnCacheHit<K, KH, V, I>) {
        m_insertion_policy->on_cache_hit(key, item);
    }

    if constexpr (detail::traits::event::HasOnCacheHit<K, KH, V, E>) {
        m_eviction_policy->on_cache_hit(key, item);
    }

    if constexpr (detail::traits::event::HasOnCacheHit<K, KH, V, C>) {
        m_constraint_policy->on_cache_hit(key, item);
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
template<class KeyView>
void Cache<K, V, I, E, C, SV, SK, KH, L>::on_cache_miss(const KeyView& key) const
{
    // Update the cache hit rate accumulators.
    m_hit_rate_acc(0);
    m_byte_hit_rate_acc(0);

    // Call event handler iif the method is defined in the policy.
    if constexpr (detail::traits::event::HasOnCacheMiss<K, KH, V, I>) {
        m_insertion_policy->on_cache_miss(key);
    }

    if constexpr (detail::traits::event::HasOnCacheMiss<K, KH, V, E>) {
        m_eviction_policy->on_cache_miss(key);
    }

    if constexpr (detail::traits::event::HasOnCacheMiss<K, KH, V, C>) {
        m_constraint_policy->on_cache_miss(key);
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void Cache<K, V, I, E, C, SV, SK, KH, L>::on_evict(const K& key, const CacheItem& item) const
{
    // Call event handler iif the method is defined in the policy.
    if constexpr (detail::traits::event::HasOnEvict<K, KH, V, I>) {
        m_insertion_policy->on_evict(key, item);
    }

    if constexpr (detail::traits::event::HasOnEvict<K, KH, V, E>) {
        m_eviction_policy->on_evict(key, item);
    }

    if constexpr (detail::traits::event::HasOnEvict<K, KH, V, C>) {
        m_constraint_policy->on_evict(key, item);
    }
}

template<class K,
         class V,
         template<class, class, class> class I,
         template<class, class, class> class E,
         template<class, class, class> class C,
         class SV,
         class SK,
         class KH,
         LockingStrategy L>
void swap(Cache<K, V, I, E, C, SV, SK, KH, L>& lhs, Cache<K, V, I, E, C, SV, SK, KH, L>& rhs) noexcept
{
    lhs.swap(rhs);
}

}  // namespace cachemere
