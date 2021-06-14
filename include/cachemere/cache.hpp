#include <vector>

#include <boost/hana.hpp>

#include "detail/traits.h"

namespace cachemere {

template<typename Key,
         typename Value,
         template<class, class>
         class InsertionPolicy,
         template<class, class>
         class EvictionPolicy,
         typename MeasureValue,
         typename MeasureKey,
         bool ThreadSafe>
Cache<Key, Value, InsertionPolicy, EvictionPolicy, MeasureValue, MeasureKey, ThreadSafe>::Cache(size_t maximum_size, uint32_t statistics_window_size)
 : m_current_size{0},
   m_maximum_size{maximum_size},
   m_statistics_window_size{statistics_window_size},
   m_insertion_policy(std::make_unique<InsertionPolicy<Key, Value>>()),
   m_eviction_policy(std::make_unique<EvictionPolicy<Key, Value>>()),
   m_measure_key{},
   m_measure_value{},
   m_mutex{},
   m_data{},
   m_hit_rate_acc(boost::accumulators::tag::rolling_window::window_size = statistics_window_size),
   m_byte_hit_rate_acc(boost::accumulators::tag::rolling_window::window_size = statistics_window_size)
{
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
template<typename C>
Cache<K, V, I, E, SV, SK, TS>::Cache(C& collection, size_t maximum_size, uint32_t statistics_window_size) : Cache(maximum_size, statistics_window_size)
{
    import(collection);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline bool Cache<K, V, I, E, SV, SK, TS>::contains(const K& key) const
{
    std::unique_lock<std::recursive_mutex> guard(lock());
    return m_data.find(key) != m_data.end();
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
std::optional<V> Cache<K, V, I, E, SV, SK, TS>::find(const K& key) const
{
    std::unique_lock<std::recursive_mutex> guard(lock());

    auto key_and_item = m_data.find(key);
    if (key_and_item != m_data.end()) {
        on_cache_hit(key_and_item->second);
        return key_and_item->second.m_value;
    }

    on_cache_miss(key);
    return std::nullopt;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
template<class C>
void Cache<K, V, I, E, SV, SK, TS>::collect_into(C& container) const
{
    using namespace boost;
    using namespace detail;

    // Use emplace_back if container is a sequence container, or emplace if container is an associative container.
    constexpr auto emplace_fn = hana::if_(
        traits::stl::has_emplace_back<C, K, V>,
        [](auto& seq_container, const auto& item) { seq_container.emplace_back(item.m_key, item.m_value); },
        [](auto& assoc_container, const auto& item) { assoc_container.emplace(item.m_key, item.m_value); });

    std::unique_lock<std::recursive_mutex> guard(lock());

    // Reserve space if the container has a reserve() method and a size method().
    hana::if_(
        hana::and_(traits::stl::has_reserve<C>, traits::stl::has_size<C>),
        [&](auto& c) { c.reserve(c.size() + m_data.size()); },
        [](auto&) {})(container);

    // Copy the cache contents to the container.
    for (const auto& [_, cached_item] : m_data) {
        emplace_fn(container, cached_item);
    }
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
bool Cache<K, V, I, E, SV, SK, TS>::insert(K key, V value)
{
    std::unique_lock<std::recursive_mutex> guard(lock());

    const auto   key_size           = static_cast<size_t>(m_measure_key(key));
    const auto   value_size         = static_cast<size_t>(m_measure_value(value));
    const size_t item_size          = key_size + value_size;
    const size_t item_size_overhead = item_size + key_size;

    const bool should_insert = compare_evict(key, item_size_overhead);

    if (should_insert) {
        insert_or_update(std::move(key), std::move(value), key_size, value_size);
    }

    return should_insert;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
bool Cache<K, V, I, E, SV, SK, TS>::remove(const K& key)
{
    std::unique_lock<std::recursive_mutex> guard(lock());

    auto key_and_item = m_data.find(key);
    if (key_and_item != m_data.end()) {
        remove(key_and_item);
        return true;
    }
    return false;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::clear()
{
    std::unique_lock<std::recursive_mutex> guard(lock());

    m_current_size = 0;
    m_data.clear();

    m_hit_rate_acc      = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);
    m_byte_hit_rate_acc = MeanAccumulator(boost::accumulators::tag::rolling_window::window_size = m_statistics_window_size);

    m_insertion_policy->clear();
    m_eviction_policy->clear();
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
template<class P>
void Cache<K, V, I, E, SV, SK, TS>::retain(P predicate_fn)
{
    std::unique_lock<std::recursive_mutex> guard(lock());

    for (auto it = m_data.begin(); it != m_data.end();) {
        const CacheItem& item = it->second;

        if (!predicate_fn(item.m_key, item.m_value)) {
            remove(it++);
        } else {
            ++it;
        }
    }
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::swap(CacheType& other)
{
    // Acquire both cache locks.
    std::unique_lock<std::recursive_mutex> me_guard(lock());
    std::unique_lock<std::recursive_mutex> other_guard(other.lock());

    using std::swap;

    swap(m_current_size, other.m_current_size);
    swap(m_maximum_size, other.m_maximum_size);
    swap(m_statistics_window_size, other.m_statistics_window_size);

    swap(m_insertion_policy, other.m_insertion_policy);
    swap(m_eviction_policy, other.m_eviction_policy);

    swap(m_measure_key, other.m_measure_key);
    swap(m_measure_value, other.m_measure_value);

    swap(m_data, other.m_data);

    swap(m_hit_rate_acc, other.m_hit_rate_acc);
    swap(m_byte_hit_rate_acc, other.m_byte_hit_rate_acc);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline size_t Cache<K, V, I, E, SV, SK, TS>::number_of_items() const
{
    std::unique_lock<std::recursive_mutex> guard(lock());
    return m_data.size();
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline size_t Cache<K, V, I, E, SV, SK, TS>::size() const
{
    std::unique_lock<std::recursive_mutex> guard(lock());
    return m_current_size;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline size_t Cache<K, V, I, E, SV, SK, TS>::maximum_size() const
{
    std::unique_lock<std::recursive_mutex> guard(lock());
    return m_maximum_size;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline void Cache<K, V, I, E, SV, SK, TS>::set_maximum_size(size_t max_size)
{
    std::unique_lock<std::recursive_mutex> guard(lock());
    m_maximum_size = max_size;

    if (m_maximum_size < m_current_size) {
        const size_t                  amount_to_free = m_current_size - m_maximum_size;
        [[maybe_unused]] const size_t amount_freed   = free_amount(amount_to_free);

        assert(amount_freed >= amount_to_free);
    }
    assert(m_current_size <= m_maximum_size);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline I<K, V>& Cache<K, V, I, E, SV, SK, TS>::insertion_policy()
{
    return *m_insertion_policy;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline E<K, V>& Cache<K, V, I, E, SV, SK, TS>::eviction_policy()
{
    return *m_eviction_policy;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline double Cache<K, V, I, E, SV, SK, TS>::hit_rate() const
{
    return boost::accumulators::rolling_mean(m_hit_rate_acc);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
inline double Cache<K, V, I, E, SV, SK, TS>::byte_hit_rate() const
{
    return boost::accumulators::rolling_mean(m_byte_hit_rate_acc);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
std::unique_lock<std::recursive_mutex> Cache<K, V, I, E, SV, SK, TS>::lock() const
{
    if constexpr (TS) {
        std::unique_lock<std::recursive_mutex> guard{m_mutex};
        return guard;
    } else {
        std::unique_lock<std::recursive_mutex> guard;
        return guard;
    }
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
template<class C>
void Cache<K, V, I, E, SV, SK, TS>::import(C& collection)
{
    std::unique_lock<std::recursive_mutex> guard(lock());

    for (auto& [key, value] : collection) {
        const auto   key_size           = static_cast<size_t>(m_measure_key(key));
        const auto   value_size         = static_cast<size_t>(m_measure_value(value));
        const size_t item_size          = key_size + value_size;
        const size_t item_size_overhead = item_size + key_size;

        if (m_current_size + item_size_overhead > m_maximum_size) {
            return;
        }

        insert_or_update(std::move(key), std::move(value), key_size, value_size);
    }
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
bool Cache<K, V, I, E, SV, SK, TS>::compare_evict(const K& candidate_key, size_t candidate_size)
{
    if (m_current_size + candidate_size <= m_maximum_size) {
        // We have enough room. Insert if the policy agrees.
        return m_insertion_policy->should_add(candidate_key);
    } else {
        if (candidate_size > m_maximum_size) {
            // The item to insert/update is bigger than the maximum size of the cache, no need to attempt an insertion.
            return false;
        }
        // We need to get a list of keys to evict to make room for the candidate. Since we restrict cache inserts by
        // cache size instead of by nb. of items, we might evict multiple items to insert one. This is bad because it
        // leads to inaccuracies:
        //
        // Say for instance we want to insert a 4 kb item. In order to insert it,
        // we might need to evict 4 1kb items.
        // Since this is a feedback cache, insertion policies operate by comparing two keys (the candidate and an
        // eviction victim) together and inserting/keeping the best one. This means that in order to insert our 4kb
        // item, we'd need to make 4 calls to the insertion policy, and insert the item only if all 4 calls come out
        // positive. *However*, this only establishes that the candidate is better than each individual replaced item,
        // not that it is better than the sum of them... This could be solved in two ways.
        //
        // The easiest way to fix the issue would be to change the cache constraint from "allocated memory" to "number
        // of cached items". This would lead to some inaccuracies in how memory is tracked, it but would
        // increase the performance of the cache (an insert would always evict [0,1] item).
        //
        // The harder way would be to change the interface of the insertion policy. Instead of should_replace(Key&,
        // Key&), we could do should_replace(Key&, vector<Key&>). This would allow the insertion policy to weigh the
        // candidate vs. *all* the evicted keys. (e.g. an LFU policy could make sure that the load counts of the
        // candidate needs to be higher than the _sum_ of load counts of all victims) I consider this method the harder
        // one because it would make the implementation of all insertion policies considerably harder. (it's not always
        // trivial to determine whether a key is a better candidate than a given set of keys)
        //
        // Those two ways could even be used together: we could change the insertion policy interface *and* provide a
        // new count-limited cache for improved performance, while keeping the memory-limited cache for when required.

        std::vector<DataMapIt> keys_to_evict;
        size_t                 freed_amount = 0;

        auto should_evict_another = [&](auto victim_it) { return victim_it != m_eviction_policy->victim_end() && freed_amount <= candidate_size; };

        for (auto victim_it = m_eviction_policy->victim_begin(); should_evict_another(victim_it); ++victim_it) {
            const K& key_to_evict = *victim_it;
            auto     key_and_item = m_data.find(key_to_evict);

            if (key_and_item != m_data.end()) {
                if (!m_insertion_policy->should_replace(key_to_evict, candidate_key)) {
                    return false;
                }
                const auto total_size_and_overhead = key_and_item->second.m_key_size + key_and_item->second.m_total_size;
                freed_amount += total_size_and_overhead;
                keys_to_evict.push_back(key_and_item);
            } else {
                // If this trips, the eviction policy tried to evict an item not in cache: the eviction policy and the
                // cache are out of sync.
                assert(false);
            }
        }

        // If this trips, something is very wrong with how size is tracked.
        // This means that evicting all items didn't free enough space in the cache for the candidate,
        // yet the check at the beginning of this method ensures that the new item is smaller than
        // the max size of the cache...
        assert((m_current_size - freed_amount) + candidate_size <= m_maximum_size);

        // Perform the eviction(s).
        for (auto key_and_item : keys_to_evict) {
            remove(key_and_item);
        }

        return true;
    }
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::insert_or_update(K&& key, V&& value, size_t key_size, size_t value_size)
{
    auto key_and_item = m_data.find(key);
    if (key_and_item != m_data.end()) {
        // Update.

        if (value_size > key_and_item->second.m_value_size) {
            // This item got bigger with the update.
            m_current_size += value_size - key_and_item->second.m_value_size;
        } else {
            // This item got smaller with the update.
            m_current_size -= key_and_item->second.m_value_size - value_size;
        }

        key_and_item->second.m_value      = std::move(value);
        key_and_item->second.m_value_size = value_size;
        key_and_item->second.m_total_size = key_size + value_size;

        on_update(key_and_item->second);
    } else {
        // Insert.
        K key_copy = key;  // Copy the key manually here so we do only a single copy.

        const auto it_and_ok = m_data.emplace(std::piecewise_construct,
                                              std::forward_as_tuple(std::move(key_copy)),
                                              std::forward_as_tuple(std::move(key), key_size, std::move(value), value_size));
        assert(it_and_ok.second);
        const CacheItem& item = it_and_ok.first->second;
        on_insert(item);

        m_current_size += key_size + key_size + value_size;
    }
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
size_t Cache<K, V, I, E, SV, SK, TS>::free_amount(size_t amount_to_free)
{
    assert(amount_to_free <= m_current_size);
    size_t freed_amount = 0;

    auto should_evict_another = [&](auto victim_it) { return victim_it != m_eviction_policy->victim_end() && freed_amount <= amount_to_free; };

    for (auto victim_it = m_eviction_policy->victim_begin(); should_evict_another(victim_it); victim_it = m_eviction_policy->victim_begin()) {
        const K& key_to_evict = *victim_it;
        auto     key_and_item = m_data.find(key_to_evict);

        if (key_and_item != m_data.end()) {
            auto total_size_and_overhead = key_and_item->second.m_key_size + key_and_item->second.m_total_size;
            freed_amount += total_size_and_overhead;
            remove(key_and_item);
        } else {
            // If this trips, the eviction policy tried to evict an item not in cache: the eviction policy and the cache are out of sync.
            assert(false);
        }
    }

    return freed_amount;
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::remove(DataMapIt it)
{
    const auto total_size_and_overhead = it->second.m_key_size + it->second.m_total_size;
    m_current_size -= total_size_and_overhead;

    on_evict(it->second.m_key);
    m_data.erase(it);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::on_insert(const CacheItem& item) const
{
    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_insert<K, V, I>,
        [&](auto& x) { return x.on_insert(item); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_insert<K, V, E>,
        [&](auto& x) { return x.on_insert(item); },
        [](auto&) {})(*m_eviction_policy);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::on_update(const CacheItem& item) const
{
    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_update<K, V, I>,
        [&](auto& x) { return x.on_update(item); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_update<K, V, E>,
        [&](auto& x) { return x.on_update(item); },
        [](auto&) {})(*m_eviction_policy);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::on_cache_hit(const CacheItem& item) const
{
    // Update the cache hit rate accumulators.
    m_hit_rate_acc(1);
    m_byte_hit_rate_acc(static_cast<uint32_t>(item.m_value_size));

    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_cachehit<K, V, I>,
        [&](auto& x) { return x.on_cache_hit(item); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_cachehit<K, V, E>,
        [&](auto& x) { return x.on_cache_hit(item); },
        [](auto&) {})(*m_eviction_policy);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::on_cache_miss(const K& key) const
{
    // Update the cache hit rate accumulators.
    m_hit_rate_acc(0);
    m_byte_hit_rate_acc(0);

    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_cachemiss<K, V, I>,
        [&](auto& x) { return x.on_cache_miss(key); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_cachemiss<K, V, E>,
        [&](auto& x) { return x.on_cache_miss(key); },
        [](auto&) {})(*m_eviction_policy);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void Cache<K, V, I, E, SV, SK, TS>::on_evict(const K& key) const
{
    // Call event handler iif the method is defined in the policy.
    boost::hana::if_(
        detail::traits::event::has_on_evict<K, V, I>,
        [&](auto& x) { return x.on_evict(key); },
        [](auto&) {})(*m_insertion_policy);

    boost::hana::if_(
        detail::traits::event::has_on_evict<K, V, E>,
        [&](auto& x) { return x.on_evict(key); },
        [](auto&) {})(*m_eviction_policy);
}

template<class K, class V, template<class, class> class I, template<class, class> class E, class SV, class SK, bool TS>
void swap(Cache<K, V, I, E, SV, SK, TS>& lhs, Cache<K, V, I, E, SV, SK, TS>& rhs)
{
    lhs.swap(rhs);
}

}  // namespace cachemere
