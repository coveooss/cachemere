# Cachemere

Cachemere is a modular header-only caching library for C++.

It provides implementations for some state-of-the-art caching algorithms,
as well as a comprehensive set of primitives for building custom caching solutions.

## Getting Started
### TinyLFU Cache
```cpp
#include <cstdint>
#include <optional>
#include <string>

#include <cachemere/cachemere.h>

using Key = int;
using Value = std::string;

// KeyMeasure and ValueMeasure are functors used by the cache to track the amount of memory used by its contents.
using KeyMeasure = cachemere::measurement::SizeOf<Key>;
using ValueMeasure = cachemere::measurement::CapacityDynamicallyAllocated<Value>;

// TinyLFUCache is an alias for Cache<policy::InsertionTinyLFU, policy::EvictionSegmentedLRU>.
using MyCache = cachemere::presets::TinyLFUCache<Key, Value, ValueMeasure, KeyMeasure>;

int main()
{
    // Cache is constrained by a maximum amount of memory.
    const size_t max_cache_size_bytes = 150;
    MyCache cache(max_cache_size_bytes);

    const bool inserted = cache.insert(42, "the answer to life, the universe, and everything");

    if (std::optional<std::string> value = cache.find(42)) {
        std::cout << "Got value for key: " << *value << std::endl;
    }

    return 0;
}
```

## Design Overview
### Background
At its core, a cache has much in common with a hash map; both are datastructures used for associating a key with a value.
The main difference between the two structures is that a cache usually introduces an additional set of constraints to prevent it
from growing too much in memory.
Some implementations restrict the _number_ of items allowed at once in the cache, others restrict the
total amount of _memory used_ by the cache, while some do away with the size constraints entirely and instead restrict the _amount of time_ an item can stay
in the cache before expiring.

A **caching scheme** is the set of algorithms used to select which items should be kept in cache over others.
Since many different metrics can be prioritized when implementing a cache, there is no _one-size-fits-all_ caching scheme (e.g. a CPU cache usually only needs to prioritize the cache hit ratio, while a web server cache also needs to consider latency on cache miss).

Because of this impossiblity to get a single well-performing and reusable caching scheme, many organizations and products fall back on using a simple [Least-Recently Used (LRU)](https://en.wikipedia.org/wiki/Cache_replacement_policies#Least_recently_used_(LRU)) cache everywhere. Although LRU is certainly _suitable_ for most use cases it is far from optimal, and using it without an afterthought might lead to excessive memory usage or degraded performance.

Cachemere is designed with the explicit goal of providing a single reusable cache that can be customized to use different schemes, allowing applications to get the maintability benefits of a single cache implementation, while at the same time getting all the performance benefits of a purpose-built cache.

### Modular Policy Design

Cachemere tackles this goal in a modular fashion with the concept of _policies_. A `cachemere::Cache` is parameterized
by an **Insertion Policy** and an **Eviction Policy**.

Broadly speaking, the job of an **Insertion Policy** is to determine whether an item should be inserted or kept in cache, while the job of an **Eviction
Policy** is to determine which item(s) should be evicted from the cache to make room for an new item. The cache will query its policies when it has to make a
decision on whether an item should be added or excluded.

This modular design allows for bi-directional code reuse and customization: the core cache implementation can be used with different policies, and the
policies can also be used with different cache implementations.

For instance, if instead of using more common policies like Least-Recently Used (LRU),
a product required the use of a custom cache that takes the frequency of access as well as the size of the item into
consideration when establishing which item to evict. To solve this, one could implement a `SizeBasedEvictionPolicy` and use it with the existing `cachemere::Cache` without issues.

Similarly, if a use case required the cache to be constrained by its number of items instead of by the amount of memory it uses (which is the current
implementation), one could implement a new `Cache` object parameterized by two policies, and reuse Cachemere's implementation of LRU with it.
