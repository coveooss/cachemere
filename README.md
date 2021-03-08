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
At its core, a cache has much in common with a hash map - both are datastructures used for associating a key with a value.
The main difference between the two structures is that a cache usually introduces an additional set of constraints to prevent it
from growing too much in memory.
Some implementations restrict the _number_ of items allowed at once in the cache, others restrict the
total amount of _memory used_ by the cache, while some do away with the size constraints entirely and instead restrict the _amount of time_ an item can stay
in the cache before expiring. With these constraints in mind, it follows that the main goal of a good caching algorithm is to identify
as accurately as possible the _best_ items to keep in cache, as well as to provide some kind of ordering mechanism to select which item
should be evicted first if need be.

### Modular Policy Design

Cachemere tries to tackle this goal in a modular fashion with the concept of _policies_. A `cachemere::Cache` is parameterized
by an **Insertion Policy** and an **Eviction Policy**.

Broadly speaking, the job of an **Insertion Policy** is to determine whether an item should be inserted or kept in cache, while the job of an **Eviction
Policy** is to determine which item(s) should be evicted from the cache to make room for an new item. The cache will query its policies when it has to make a
decision on whether an item should be added or excluded.

This modular design allows for bi-directional code reuse and customization: the core cache implementation can be used with different policies, and the
policies can also be used with different cache implementations.

For instance, if instead of using more common policies like Least-Recently Used (LRU)
or Least-Frequently Used (LFU), you wanted to implement a custom cache that takes the frequency of access **as well as** the size of the item into
consideration when establishing which item to evict, you could simply implement a `SizeBasedEvictionPolicy` and use it with the existing `cachemere::Cache`.

Similarly, if a use case required the cache to be constrained by its number of items instead of by the amount of memory it uses (which is the current
implementation), one could implement a new `Cache` object parameterized by two policies, and re-use Cachemere's implementation of LRU, for instance.
