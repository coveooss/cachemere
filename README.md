# Cachemere

Cachemere is a modular header-only caching library for C++.

It provides implementations for some state-of-the-art caching algorithms,
as well as a comprehensive set of primitives for building custom caching solutions.

[![Tests](https://github.com/coveooss/cachemere/actions/workflows/test.yml/badge.svg)](https://github.com/coveooss/cachemere/actions/workflows/test.yml)
![License](https://img.shields.io/github/license/coveooss/cachemere)

## Getting Started

### Requirements

* A C++20 compiler: the cache and its policies are constrained with concepts.
* [Abseil](https://abseil.io/) and [Boost](https://www.boost.org/) (Accumulators and Dynamic Bitset).
* CMake 3.20 or later to build the tests and benchmarks.

### Upgrading from 0.3

Cachemere 0.4 has these breaking changes:

* **C++20 is required** (0.3 needed C++17): the template parameters are constrained with concepts, which replace Boost.Hana.
  Boost.Hana is no longer a dependency, and the `cachemere` CMake target now requires `cxx_std_20`.
* **The `bool ThreadSafe` template parameter is replaced by `LockingStrategy Locking`**, on `cachemere::Cache` and on the presets:
  * `ThreadSafe = true` becomes `LockingStrategy::Mutex` (still the default).
  * `ThreadSafe = false` becomes `LockingStrategy::None`.
  * `LockingStrategy::TimedMutex` is new: see `try_find()` and `try_insert()` below.
* **The policy interfaces changed.** Custom policies need to be migrated:
  * Eviction policies: `victim_begin()` and `victim_end()` are replaced by `Ticket pop_victim()` and `void rollback(Ticket)`.
    The cache pops victims until the constraint is satisfied, then commits them, or rolls all of them back if the insertion is abandoned.
    A committed victim doesn't get an `on_evict()` call anymore: eviction policies only get `on_evict()` for `remove()` and `retain()`.
  * Constraint policies: `can_add()` and `can_replace()` are replaced by `prepare_insert()` and `prepare_replace()`, which return
    tickets with `is_satisfiable()`, `is_satisfied()` and `register_eviction()`.
  * Insertion policies are unchanged.

  See [Implementing Custom Policies](https://coveooss.github.io/cachemere/implementingCustomPolicies.html) for the details.
* **The `.hpp` implementation headers are removed**: every method is now defined in its `.h` header. Include the `.h` headers, or `cachemere.h`.
* **Keys must satisfy the `cachemere::CacheKey` concept**: move-constructible and comparable with `==`. They don't need to be copyable or assignable.
* **Visitors, predicates and factories are checked by concepts**: `for_each()` takes a visitor callable with `const Key&` and `const Value&`
  (its result is ignored), `retain()` a predicate whose result converts to `bool`, and `find_or_insert()` a factory whose result converts to `Value`.
* **Caches are neither copyable nor movable**, whatever the locking strategy, as in 0.3. Use `swap()` to exchange the contents of two caches.

### Documentation Highlights

#### Custom Keys

Cachemere supports using arbitrary types as keys. The page
on [Using Custom Keys](https://coveooss.github.io/cachemere/customKeys.html) covers this in greater detail.

#### Heterogeneous Lookup

Like `std::map`, `cachemere::Cache` supports heterogeneous lookup. Take a look
at [the documentation on heterogeneous lookup](https://coveooss.github.io/cachemere/heterogeneousLookup.html) for more
details and usage instructions.

#### Locking

By default, every cache operation is protected by a `std::recursive_mutex`. The last template parameter of
`cachemere::Cache` and of the presets selects another `cachemere::LockingStrategy`:

* `LockingStrategy::Mutex` (default): blocking operations, plus `try_find()` and `try_insert()`, which return
  immediately when another thread holds the lock.
* `LockingStrategy::TimedMutex`: also adds `try_find()` and `try_insert()` overloads that wait for up to a timeout.
* `LockingStrategy::None`: no locking at all, for caches used by a single thread.

### TinyLFU Cache Example

```cpp
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include <cachemere.h>

using Key = int;
using Value = std::string;

// KeyMeasure and ValueMeasure are functors used by the cache to track the amount of memory used by its contents.
using KeyMeasure = cachemere::measurement::SizeOf<Key>;
using ValueMeasure = cachemere::measurement::CapacityDynamicallyAllocated<Value>;

// TinyLFUCache is an alias for Cache<policy::InsertionTinyLFU, policy::EvictionSegmentedLRU>.
using MyCache = cachemere::presets::memory::TinyLFUCache<Key, Value, ValueMeasure, KeyMeasure>;

int main()
{
    // Cache is constrained by a maximum amount of memory.
    const size_t max_cache_size_bytes = 150;
    MyCache cache(max_cache_size_bytes);

    if (!cache.insert(42, "the answer to life, the universe, and everything")) {
        std::cout << "The cache refused the item" << std::endl;
    }

    if (std::optional<std::string> value = cache.find(42)) {
        std::cout << "Got value for key: " << *value << std::endl;
    }

    // Look the key up, and only compute the value on a miss.
    const std::string value = cache.find_or_insert(7, [](const Key& key) { return std::to_string(key); });
    std::cout << "Value for key 7: " << value << std::endl;

    return 0;
}
```

For more information, take a look at the [full documentation](https://coveooss.github.io/cachemere/).

## Design Overview

### Background

At its core, a cache has much in common with a hash map; both are data structures used for associating a key with a
value.
The main difference between the two structures is that a cache usually introduces an additional set of constraints to
prevent it
from growing too much in memory.
Some implementations restrict the _number_ of items allowed at once in the cache, others restrict the
total amount of _memory used_ by the cache, while some do away with the size constraints entirely and instead restrict
the _amount of time_ an item can stay
in the cache before expiring.

A **caching scheme** is the set of algorithms used to select which items should be kept in cache over others.
Since many different metrics can be prioritized when implementing a cache, there is no _one-size-fits-all_ caching
scheme (e.g. a CPU cache usually only needs to prioritize the cache hit ratio, while a web server cache also needs to
consider latency on cache miss).

Because of this impossiblity of relying on a single well-performing and reusable caching scheme, many organizations and
products fall back on using a
simple [Least-Recently Used (LRU)](https://en.wikipedia.org/wiki/Cache_replacement_policies#Least_recently_used_(LRU))
cache everywhere. Although LRU is certainly _suitable_ for most use cases it is far from optimal, and using it without
an afterthought might lead to excessive memory usage or degraded performance.

Cachemere is designed with the explicit goal of providing a single reusable cache that can be customized using different
schemes, allowing applications to get the maintainability benefits of a single cache implementation, while at the same
time getting all the performance benefits of a purpose-built cache.

### Modular Policy Design

Cachemere tackles this goal in a modular fashion with the concept of _policies_. A `cachemere::Cache` is parameterized
by a **Constraint Policy**, an **Insertion Policy**, and an **Eviction Policy**.

Broadly speaking, the job of the **Constraint Policy** is to determine whether an item _can_ be in the cache (e.g. if
there is enough free space), the job of an **Insertion Policy** is to determine whether an item should be inserted or
kept in cache, while the job of an **Eviction
Policy** is to determine which item(s) should be evicted from the cache. The cache will query its policies when it has
to make a
decision on whether an item should be added or excluded.

This modular design allows for bi-directional code reuse and customization: the core cache implementation can be used
with different policies, and the
policies can also be used with different cache implementations.

For instance, instead of using more common policies like Least-Recently Used (LRU),
a product might need to use a custom cache that takes the frequency of access as well as the size of the item into
consideration when establishing which item to evict. To do this, one could implement a `SizeBasedEvictionPolicy` and use
it with the existing `cachemere::Cache`.
