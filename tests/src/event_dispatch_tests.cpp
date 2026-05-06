#include <gtest/gtest.h>

#include <cassert>

#include "cachemere/cache.h"
#include "cachemere/item.h"
#include "cachemere/measurement.h"

namespace {

struct EventCounters {
    static void reset()
    {
        insertion_hits  = 0;
        eviction_hits   = 0;
        constraint_hits = 0;
    }

    inline static int insertion_hits  = 0;
    inline static int eviction_hits   = 0;
    inline static int constraint_hits = 0;
};

template<typename Key, typename KeyHash, typename Value> class TrackingInsertionPolicy
{
public:
    using CacheItem = cachemere::Item<Value>;

    void clear() {}

    bool should_add(const Key&) const
    {
        return true;
    }

    bool should_replace(const Key&, const Key&) const
    {
        return true;
    }

    void on_cache_hit(const Key&, const CacheItem&)
    {
        ++EventCounters::insertion_hits;
    }
};

template<typename Key, typename KeyHash, typename Value> class TrackingEvictionPolicy
{
public:
    using CacheItem = cachemere::Item<Value>;
    class VictimIterator
    {
    public:
        const Key& operator*() const
        {
            assert(false);
            return *static_cast<const Key*>(nullptr);
        }

        VictimIterator& operator++()
        {
            return *this;
        }

        bool operator==(const VictimIterator&) const
        {
            return true;
        }

        bool operator!=(const VictimIterator&) const
        {
            return false;
        }
    };

    void clear() {}

    void on_insert(const Key&, const CacheItem&) {}

    void on_cache_hit(const Key&, const CacheItem&)
    {
        ++EventCounters::eviction_hits;
    }

    void on_evict(const Key&, const CacheItem&) {}

    [[nodiscard]] VictimIterator victim_begin() const
    {
        return {};
    }

    [[nodiscard]] VictimIterator victim_end() const
    {
        return {};
    }
};

template<typename Key, typename KeyHash, typename Value> class TrackingConstraintPolicy
{
public:
    using CacheItem = cachemere::Item<Value>;

    void clear() {}

    bool can_add(const Key&, const CacheItem&) const
    {
        return true;
    }

    bool can_replace(const Key&, const CacheItem&, const CacheItem&) const
    {
        return true;
    }

    void on_insert(const Key&, const CacheItem&) {}

    void on_update(const Key&, const CacheItem&, const CacheItem&) {}

    void on_cache_hit(const Key&, const CacheItem&)
    {
        ++EventCounters::constraint_hits;
    }

    void on_evict(const Key&, const CacheItem&) {}
};

using TrackingCache = cachemere::Cache<int,
                                       int,
                                       TrackingInsertionPolicy,
                                       TrackingEvictionPolicy,
                                       TrackingConstraintPolicy,
                                       cachemere::measurement::SizeOf<int>,
                                       cachemere::measurement::SizeOf<int>>;

}  // namespace

TEST(CacheEventDispatch, CacheHitIsDeliveredOncePerPolicy)
{
    EventCounters::reset();

    TrackingCache cache;
    ASSERT_TRUE(cache.insert(1, 10));

    const auto value = cache.find(1);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(10, *value);

    EXPECT_EQ(1, EventCounters::insertion_hits);
    EXPECT_EQ(1, EventCounters::eviction_hits);
    EXPECT_EQ(1, EventCounters::constraint_hits);
}
