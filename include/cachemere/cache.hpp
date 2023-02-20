namespace cachemere {

template<typename Key,
         typename Value,
         template<class, class, class>
         class InsertionPolicy,
         template<class, class, class>
         class EvictionPolicy,
         template<class, class, class>
         class ConstraintPolicy,
         typename MeasureValue,
         typename MeasureKey,
         typename KeyHash,
         bool ThreadSafe>
template<typename... Args>
Cache<Key, Value, InsertionPolicy, EvictionPolicy, ConstraintPolicy, MeasureValue, MeasureKey, KeyHash, ThreadSafe>::Cache(Args... args)
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
template<typename Coll, typename... Args>
Cache<K, V, I, E, C, SV, SK, KH, TS>::Cache(Coll& collection, std::tuple<Args...> args)
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
template<typename KeyView>
inline bool Cache<K, V, I, E, C, SV, SK, KH, TS>::contains(const KeyView& key) const
{
    LockGuard guard(lock());
    return m_data.find(key) != m_data.end();
}

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
template<typename KeyView>
std::optional<V> Cache<K, V, I, E, C, SV, SK, KH, TS>::find(const KeyView& key) const
{
    LockGuard guard(lock());

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
template<class Container>
void Cache<K, V, I, E, C, SV, SK, KH, TS>::collect_into(Container& container) const
{
    using namespace detail;

    // Use emplace_back if container is a sequence container, or emplace if container is an associative container.
    constexpr auto emplace_fn = boost::hana::if_(
        traits::stl::has_emplace_back<Container, K, V>,
        [](auto& seq_container, const auto& key, const auto& item) { seq_container.emplace_back(key, item.m_value); },
        [](auto& assoc_container, const auto& key, const auto& item) { assoc_container.emplace(key, item.m_value); });

    LockGuard guard(lock());

    // Reserve space if the container has a reserve() method and a size method().
    boost::hana::if_(
        boost::hana::and_(traits::stl::has_reserve<Container>, traits::stl::has_size<Container>),
        [&](auto& c) { c.reserve(c.size() + m_data.size()); },
        [](auto&) {})(container);

    // Copy the cache contents to the container.
    for (const auto& [key, cached_item] : m_data) {
        emplace_fn(container, key, cached_item);
    }
}

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
bool Cache<K, V, I, E, C, SV, SK, KH, TS>::insert(K key, V value)
{
    LockGuard guard(lock());

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
bool Cache<K, V, I, E, C, SV, SK, KH, TS>::remove(const K& key)
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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::clear()
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
template<class P>
void Cache<K, V, I, E, C, SV, SK, KH, TS>::retain(P predicate_fn)
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
template<class F>
void Cache<K, V, I, E, C, SV, SK, KH, TS>::for_each(F unary_function)
{
    LockGuard guard(lock());
    for (const auto& [key, value] : m_data) {
        unary_function(key, value.m_value);
    }
}

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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::swap(CacheType& other) noexcept
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
        // The only exception that can sensibly be thrown in the above block is a `system_error` when acquiring the mutexes of both caches (if the caches are
        // running in thread-safe mode).
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
inline size_t Cache<K, V, I, E, C, SV, SK, KH, TS>::number_of_items() const
{
    LockGuard guard(lock());
    return m_data.size();
}

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
template<typename... Args>
void Cache<K, V, I, E, C, SV, SK, KH, TS>::update_constraint(Args... args)
{
    LockGuard guard(lock());
    m_constraint_policy->update(std::forward<Args>(args)...);

    auto should_evict_another = [&](auto victim_it) { return victim_it != m_eviction_policy->victim_end() && !m_constraint_policy->is_satisfied(); };

    for (auto victim_it = m_eviction_policy->victim_begin(); should_evict_another(victim_it); victim_it = m_eviction_policy->victim_begin()) {
        const K& key_to_evict = *victim_it;
        auto     key_and_item = m_data.find(key_to_evict);

        if (key_and_item != m_data.end()) {
            remove(key_and_item);
        } else {
            // If this trips, the eviction policy tried to evict an item not in cache: the eviction policy and the cache are out of sync.
            assert(false);
        }
    }

    assert(m_constraint_policy->is_satisfied());
}

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
inline auto Cache<K, V, I, E, C, SV, SK, KH, TS>::insertion_policy() -> MyInsertionPolicy&
{
    return *m_insertion_policy;
}

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
inline auto Cache<K, V, I, E, C, SV, SK, KH, TS>::insertion_policy() const -> const MyInsertionPolicy&
{
    return *m_insertion_policy;
}

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
inline auto Cache<K, V, I, E, C, SV, SK, KH, TS>::eviction_policy() -> MyEvictionPolicy&
{
    return *m_eviction_policy;
}

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
inline auto Cache<K, V, I, E, C, SV, SK, KH, TS>::eviction_policy() const -> const MyEvictionPolicy&
{
    return *m_eviction_policy;
}

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
inline auto Cache<K, V, I, E, C, SV, SK, KH, TS>::constraint_policy() -> MyConstraintPolicy&
{
    return *m_constraint_policy;
}

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
inline auto Cache<K, V, I, E, C, SV, SK, KH, TS>::constraint_policy() const -> const MyConstraintPolicy&
{
    return *m_constraint_policy;
}

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
inline double Cache<K, V, I, E, C, SV, SK, KH, TS>::hit_rate() const
{
    return boost::accumulators::rolling_mean(m_hit_rate_acc);
}

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
inline double Cache<K, V, I, E, C, SV, SK, KH, TS>::byte_hit_rate() const
{
    return boost::accumulators::rolling_mean(m_byte_hit_rate_acc);
}

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
inline uint32_t Cache<K, V, I, E, C, SV, SK, KH, TS>::statistics_window_size() const
{
    return m_statistics_window_size;
}

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
inline void Cache<K, V, I, E, C, SV, SK, KH, TS>::statistics_window_size(uint32_t window_size)
{
    m_statistics_window_size = window_size;

    m_hit_rate_acc      = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);
    m_byte_hit_rate_acc = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);
}

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
auto Cache<K, V, I, E, C, SV, SK, KH, TS>::lock() const -> LockGuard
{
    if constexpr (TS) {
        LockGuard guard{m_mutex};
        return guard;
    } else {
        LockGuard guard;
        return guard;
    }
}

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
auto Cache<K, V, I, E, C, SV, SK, KH, TS>::lock([[maybe_unused]] std::defer_lock_t defer_lock_tag) const -> LockGuard
{
    if constexpr (TS) {
        LockGuard guard{m_mutex, defer_lock_tag};
        return guard;
    } else {
        LockGuard guard;
        return guard;
    }
}

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
auto Cache<K, V, I, E, C, SV, SK, KH, TS>::lock_pair(CacheType& other) const -> std::pair<LockGuard, LockGuard>
{
    LockGuard my_guard    = lock(std::defer_lock);
    LockGuard other_guard = other.lock(std::defer_lock);

    if constexpr (TS) {  // std::lock throws if any of the guards don't refer to a mutex.
        std::lock(my_guard, other_guard);
    }

    return std::make_pair<LockGuard, LockGuard>(std::move(my_guard), std::move(other_guard));
}

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
template<class Coll>
void Cache<K, V, I, E, C, SV, SK, KH, TS>::import(Coll& collection)
{
    LockGuard guard(lock());

    for (auto& [key, value] : collection) {
        const auto key_size   = static_cast<size_t>(m_measure_key(key));
        const auto value_size = static_cast<size_t>(m_measure_value(value));
        CacheItem  item{key_size, std::move(value), value_size};

        if (!m_constraint_policy->can_add(key, item)) {
            return;
        }

        insert_or_update(std::move(key), std::move(item));
    }
}

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
bool Cache<K, V, I, E, C, SV, SK, KH, TS>::check_insert(const K& key, const CacheItem& item)
{
    if (m_constraint_policy->can_add(key, item)) {
        return m_insertion_policy->should_add(key);
    }

    // We need to perform some evictions to try and make some room.
    // Since the insertion process can fail at anytime before we know how many keys to evict (e.g. if should_replace was to return false) however,
    // we can't directly evict items as we go. To fix this, we copy the constraint policy to see how many keys we'd have to evict. If we manage to
    // satisfy the constraint copy, we evict the keys we picked and proceed with the insertion.
    auto                   constraint_copy = std::make_unique<MyConstraintPolicy>(*m_constraint_policy);
    std::vector<DataMapIt> keys_to_evict;

    auto should_evict_another = [&](auto victim_it) { return victim_it != m_eviction_policy->victim_end() && !constraint_copy->can_add(key, item); };

    // As long as the constraint isn't satisfied, we keep evicting keys.
    for (auto victim_it = m_eviction_policy->victim_begin(); should_evict_another(victim_it); ++victim_it) {
        const K& key_to_evict = *victim_it;
        auto     key_and_item = m_data.find(key_to_evict);

        if (key_and_item != m_data.end()) {
            if (!m_insertion_policy->should_replace(key_to_evict, key)) {
                // This key to evict is considered "better" to have in cache than the item to be inserted.
                // In this case, we abort the insertion.
                return false;
            }

            constraint_copy->on_evict(key_and_item->first, key_and_item->second);
            keys_to_evict.push_back(key_and_item);
        } else {
            // If this trips, the eviction policy tried to evict an item not in cache: the eviction policy and the
            // cache are out of sync.
            assert(false);
        }
    }

    if (constraint_copy->can_add(key, item)) {
        // The constraint is happy with that, so we actually evict the collected keys.
        for (auto key_and_item : keys_to_evict) {
            remove(key_and_item);
        }
        return true;
    }

    return false;
}

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
bool Cache<K, V, I, E, C, SV, SK, KH, TS>::check_replace(const K& key, const CacheItem& old_item, const CacheItem& new_item)
{
    if (m_constraint_policy->can_replace(key, old_item, new_item)) {
        return true;
    }

    // The logic here is very similar to check_insert but with an important modification.
    // Since in this code path we're updating an existing key instead of adding a new one, we need to handle the case
    // where the eviction policy recommends the eviction of the key we're trying to insert. If this happens and the constraint
    // is still not satisfied afterwards, we need to treat all subsequent constraint checks as if we were inserting instead of updating our key
    // (because the original was evicted).
    auto constraint_copy      = std::make_unique<MyConstraintPolicy>(*m_constraint_policy);
    bool evicted_original_key = false;

    auto can_replace = [&]() {
        if (evicted_original_key) {
            return constraint_copy->can_add(key, new_item);
        } else {
            return constraint_copy->can_replace(key, old_item, new_item);
        }
    };

    auto should_evict_another = [&](auto victim_it) { return victim_it != m_eviction_policy->victim_end() && !can_replace(); };

    // Evict until the constraint copy is happy.
    std::vector<DataMapIt> keys_to_evict;

    for (auto victim_it = m_eviction_policy->victim_begin(); should_evict_another(victim_it); ++victim_it) {
        const K& key_to_evict = *victim_it;
        auto     key_and_item = m_data.find(key_to_evict);

        if (key_and_item != m_data.end()) {
            if (!m_insertion_policy->should_replace(key_to_evict, key)) {
                return false;
            }

            if (!evicted_original_key) {
                evicted_original_key = key_and_item->first == key;
            }

            constraint_copy->on_evict(key_and_item->first, key_and_item->second);
            keys_to_evict.push_back(key_and_item);
        } else {
            // If this trips, the eviction policy tried to evict an item not in cache: the eviction policy and the
            // cache are out of sync.
            assert(false);
        }
    }

    if (can_replace()) {
        for (auto key_and_item : keys_to_evict) {
            remove(key_and_item);
        }
        return true;
    }

    return false;
}

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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::insert_or_update(K&& key, CacheItem&& item)
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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::remove(DataMapIt it)
{
    on_evict(it->first, it->second);
    m_data.erase(it);
}

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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::on_insert(const K& key, const CacheItem& item) const
{
    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_insert<K, KH, V, I>,
        [&](auto& x) { return x.on_insert(key, item); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_insert<K, KH, V, E>,
        [&](auto& x) { return x.on_insert(key, item); },
        [](auto&) {})(*m_eviction_policy);

    boost::hana::if_(
        detail::traits::event::has_on_insert<K, KH, V, C>,
        [&](auto& x) { return x.on_insert(key, item); },
        [](auto&) {})(*m_constraint_policy);
}

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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::on_update(const K& key, const CacheItem& old_item, const CacheItem& new_item) const
{
    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_update<K, KH, V, I>,
        [&](auto& x) { return x.on_update(key, old_item, new_item); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_update<K, KH, V, E>,
        [&](auto& x) { return x.on_update(key, old_item, new_item); },
        [](auto&) {})(*m_eviction_policy);

    boost::hana::if_(
        detail::traits::event::has_on_update<K, KH, V, C>,
        [&](auto& x) { return x.on_update(key, old_item, new_item); },
        [](auto&) {})(*m_constraint_policy);
}

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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::on_cache_hit(const K& key, const CacheItem& item) const
{
    // Update the cache hit rate accumulators.
    m_hit_rate_acc(1);
    m_byte_hit_rate_acc(static_cast<uint32_t>(item.m_value_size));

    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_cachehit<K, KH, V, I>,
        [&](auto& x) { return x.on_cache_hit(key, item); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_cachehit<K, KH, V, E>,
        [&](auto& x) { return x.on_cache_hit(key, item); },
        [](auto&) {})(*m_eviction_policy);

    boost::hana::if_(
        detail::traits::event::has_on_cachehit<K, KH, V, E>,
        [&](auto& x) { return x.on_cache_hit(key, item); },
        [](auto&) {})(*m_eviction_policy);
}

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
template<class KeyView>
void Cache<K, V, I, E, C, SV, SK, KH, TS>::on_cache_miss(const KeyView& key) const
{
    // Update the cache hit rate accumulators.
    m_hit_rate_acc(0);
    m_byte_hit_rate_acc(0);

    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_cachemiss<K, KH, V, I>,
        [&](auto& x) { return x.on_cache_miss(key); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_cachemiss<K, KH, V, E>,
        [&](auto& x) { return x.on_cache_miss(key); },
        [](auto&) {})(*m_eviction_policy);

    boost::hana::if_(
        detail::traits::event::has_on_cachemiss<K, KH, V, C>,
        [&](auto& x) { return x.on_cache_miss(key); },
        [](auto&) {})(*m_constraint_policy);
}

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
void Cache<K, V, I, E, C, SV, SK, KH, TS>::on_evict(const K& key, const CacheItem& item) const
{
    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_evict<K, KH, V, I>,
        [&](auto& x) { return x.on_evict(key, item); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_evict<K, KH, V, E>,
        [&](auto& x) { return x.on_evict(key, item); },
        [](auto&) {})(*m_eviction_policy);

    boost::hana::if_(
        detail::traits::event::has_on_evict<K, KH, V, C>,
        [&](auto& x) { return x.on_evict(key, item); },
        [](auto&) {})(*m_constraint_policy);
}

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
void swap(Cache<K, V, I, E, C, SV, SK, KH, TS>& lhs, Cache<K, V, I, E, C, SV, SK, KH, TS>& rhs) noexcept
{
    lhs.swap(rhs);
}

}  // namespace cachemere
