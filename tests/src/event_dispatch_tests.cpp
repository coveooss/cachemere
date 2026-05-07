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
    struct Ticket
    {
    public:
        [[nodiscard]] const Key& key() const
        {
            assert(false);
            return *static_cast<const Key*>(nullptr);
        }
    };

    void clear() {}

    void on_insert(const Key&, const CacheItem&) {}

    void on_cache_hit(const Key&, const CacheItem&)
    {
        ++EventCounters::eviction_hits;
    }

    void on_evict(const Key&, const CacheItem&) {}

    [[nodiscard]] Ticket pop_victim()
    {
        return {};
    }

    void rollback(Ticket)
    {
    }
};

template<typename Key, typename KeyHash, typename Value> class TrackingConstraintPolicy
{
public:
    using CacheItem = cachemere::Item<Value>;
    struct Ticket
    {
        [[nodiscard]] bool is_satisfiable() const
        {
            return true;
        }

        [[nodiscard]] bool is_satisfied() const
        {
            return true;
        }

        void register_eviction(const Key&, const CacheItem&) {}
    };

    void clear() {}

    [[nodiscard]] Ticket prepare_insert(const Key&, const CacheItem&) const
    {
        return {};
    }

    [[nodiscard]] Ticket prepare_replace(const Key&, const CacheItem&, const CacheItem&) const
    {
        return {};
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
