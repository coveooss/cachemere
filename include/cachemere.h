#include "cachemere/policy/eviction_lru.h"
#include "cachemere/policy/eviction_segmented_lru.h"

#include "cachemere/policy/insertion_always.h"
#include "cachemere/policy/insertion_tinylfu.h"

#include "cachemere/cache.h"
#include "cachemere/measurement.h"
#include "cachemere/presets.h"

/*! \mainpage Cachemere
 *
 * Cachemere is a modular header-only caching library for C++.
 *
 * It provides implementations for some state-of-the-art caching algorithms,
 * as well as a comprehensive set of primitives for building custom caching solutions.
 *
 * ## Getting Started
 * ### TinyLFU Cache
 * ```
 * #include <cstdint>
 * #include <optional>
 * #include <string>
 *
 * #include <cachemere/cachemere.h>
 *
 * using Key = int;
 * using Value = std::string;
 *
 * // KeyMeasure and ValueMeasure are functors used by the cache to track the amount of memory used by its contents.
 * using KeyMeasure = cachemere::measurement::SizeOf<Key>;
 * using ValueMeasure = cachemere::measurement::CapacityDynamicallyAllocated<Value>;
 *
 * // TinyLFUCache is an alias for Cache<policy::InsertionTinyLFU, policy::EvictionSegmentedLRU>.
 * using MyCache = cachemere::presets::TinyLFUCache<Key, Value, ValueMeasure, KeyMeasure>;
 *
 * int main()
 * {
 *     // Cache is constrained by a maximum amount of memory.
 *     const size_t max_cache_size_bytes = 150;
 *     MyCache cache(max_cache_size_bytes);
 *
 *     const bool inserted = cache.insert(42, "the answer to life, the universe, and everything");
 *
 *     if (std::optional<std::string> value = cache.find(42)) {
 *         std::cout << "Got value for key: " << *value << std::endl;
 *     }
 *
 *     return 0;
 * }
 * ```
 *
 * ## Design Overview
 * ### Background
 * At its core, a cache has much in common with a hash map - both are datastructures used for associating a key with a value.
 * The main difference between the two structures is that a cache usually introduces an additional set of constraints to prevent it
 * from growing too much in memory.
 * Some implementations restrict the _number_ of items allowed at once in the cache, others restrict the
 * total amount of _memory used_ by the cache, while some do away with the size constraints entirely and instead restrict the _amount of time_ an item can stay
 * in the cache before expiring. With these constraints in mind, it follows that the main goal of a good caching algorithm is to identify
 * as accurately as possible the _best_ items to keep in cache, as well as to provide some kind of ordering mechanism to select which item
 * should be evicted first if need be.
 *
 * ### Modular Policy Design
 *
 * Cachemere tries to tackle this goal in a modular fashion with the concept of _policies_. A cachemere::Cache is parameterized
 * by an **Insertion Policy** and an **Eviction Policy**.
 *
 * Broadly speaking, the job of an **Insertion Policy** is to determine whether an item should be inserted or kept in cache, while the job of an **Eviction
 * Policy** is to determine which item(s) should be evicted from the cache to make room for an new item. The cache will query its policies when it has to make a
 * decision on whether an item should be added or excluded.
 *
 * This modular design allows for bi-directional code reuse and customization: the core cache implementation can be used with different policies, and the
 * policies can also be used with different cache implementations.
 *
 * For instance, if instead of using more common policies like Least-Recently Used (LRU)
 * or Least-Frequently Used (LFU), you wanted to implement a custom cache that takes the frequency of access **as well as** the size of the item into
 * consideration when establishing which item to evict, you could simply implement a `SizeBasedEvictionPolicy` and use it with the existing cachemere::Cache.
 *
 * Similarly, if a use case required the cache to be constrained by its number of items instead of by the amount of memory it uses (which is the current
 * implementation), one could implement a new `Cache` object parameterized by two policies, and re-use Cachemere's implementation of LRU, for instance.
 *
 * See \ref implementingCustomPolicies for more details about policy implementation.
 *
 */

/*! \page implementingCustomPolicies Implementing Custom Policies
 * This page is about implementing custom policies.
 * ## Shared Characteristics
 * ### Construction Requirements
 * Both types of policies _must_ be default-constructible, because the cache is responsible for instanciating them.
 * ### Event Handlers
 * The policies _may_ define the following event handlers to be notified of changes in the cache. These handlers will only be called if implemented.
 * * `void on_insert(cachemere::detail::Item<K, V>& item)`
 *
 *      Called immediately after a successful insertion in cache.
 *
 * * `void on_update(cachemere::detail::Item<K, V>& item)`
 *
 *      Called immediately after an insertion of an item that was already in cache (which has the effect of updating its value).
 *
 * * `void on_cache_hit(cachemere::detail::Item<K, V>& item)`
 *
 *      Called immediately after a cache hit (a successful call to `cachemere::Cache::find`).
 *
 * * `void on_cache_miss(K& key)`
 *
 *      Called immediately after a cache miss (an unsuccessful call to `cachemere::Cache::find`).
 *
 * * `void on_evict(K& key)`
 *
 *      Called immediately _before_ dropping the provided key from the cache. The call is done this way to allow policies to keep references to cached items
 * instead of copies.
 *
 * ### Lifetime Requirements
 * Policies can safely keep references to object **keys** given that they properly drop all references to that key upon receiving an `on_evict` event.
 *
 * Policies can safely keep references to object **values** given that they properly drop all references to that value upon receiving an `on_evict` event.
 * In addition, policies keeping references to values **must** swap the held reference with the updated reference when receiving an `on_update` event.
 *
 * @warning Keeping references to cache-owned keys or values without respecting the constraints above will result in undefined behavior.
 *
 * ### Concurrency Requirements
 * Policies do not need to take any steps to ensure thread-safety. The cache will properly protect policies to ensure no concurrent operations occur.
 *
 * ## Insertion Policy
 *
 * In addition to the shared requirements above, an insertion policy must implement the following methods in order to be valid:
 *
 * * `bool should_add(const Key& key)`
 *
 *      Should return `true` if the policy allows the specified key to be inserted in cache.
 *      @note Returning `true` guarantees that the object matching the key will be inserted in cache.
 *
 * * `bool should_replace(const Key& victim, const Key& candidate)`
 *
 *      Should return `true` if the policy believes that the candidate should replace the victim in cache.
 *      @note Returning `true` guarantees that the object matching the `candidate` key will be inserted in cache.
 *
 * ## Eviction Policy
 *
 * In addition to the shared requirements above, an eviction policy must implement the following methods in order to be valid:
 *
 *
 * * `Iterator victim_begin() const`
 *
 *      Should return an iterator pointing to the first _key_ that should be evicted from the cache.
 *      @warning Returning a key from this iterator does not guarantee that the item will be evicted.
 *            The policy should only rely on event handlers to track which items are currently in cache.
 *
 * * `Iterator victim_end() const`
 *
 *      Should return an end iterator. Used to determine whether victim iteration is complete.
 */
