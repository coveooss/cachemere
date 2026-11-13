#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

#include <absl/hash/hash.h>

#include "cachemere/cache.h"
#include "cachemere/item.h"
#include "cachemere/measurement.h"
#include "cachemere/policy/bind.h"
#include "cachemere/policy/constraint_time_to_live.h"
#include "cachemere/policy/eviction_lru.h"
#include "cachemere/policy/eviction_segmented_lru.h"
#include "cachemere/policy/insertion_always.h"

using namespace cachemere;
using namespace std::chrono_literals;

namespace {

// A clock that only moves when a test advances it, so expirations don't depend on sleeps.
struct ManualClock {
    using duration   = std::chrono::milliseconds;
    using rep        = duration::rep;
    using period     = duration::period;
    using time_point = std::chrono::time_point<ManualClock, duration>;

    static constexpr bool is_steady = true;

    static time_point now() noexcept
    {
        return time_point{s_elapsed};
    }

    static void advance(duration elapsed)
    {
        s_elapsed += elapsed;
    }

    inline static duration s_elapsed{};
};

using TestItem       = Item<uint32_t>;
using TestConstraint = policy::BasicConstraintTimeToLive<uint32_t, absl::Hash<uint32_t>, uint32_t, ManualClock>;

using TestCache = Cache<uint32_t,
                        uint32_t,
                        policy::InsertionAlways,
                        policy::EvictionLRU,
                        policy::bind<policy::BasicConstraintTimeToLive, ManualClock>::ttype,
                        measurement::SizeOf<uint32_t>,
                        measurement::SizeOf<uint32_t>>;

}  // namespace

TEST(ConstraintTimeToLive, ItemExpiresOnceTimeToLiveHasPassed)
{
    TestConstraint constraint{100ms};
    const uint32_t key = 1;
    constraint.on_insert(key, TestItem{4, 42, 4});

    ManualClock::advance(100ms);
    EXPECT_FALSE(constraint.is_expired(key));

    ManualClock::advance(1ms);
    EXPECT_TRUE(constraint.is_expired(key));
}

TEST(ConstraintTimeToLive, UpdateAndCacheHitRestartTimeToLive)
{
    TestConstraint constraint{100ms};
    const uint32_t key = 1;
    constraint.on_insert(key, TestItem{4, 42, 4});

    // The update restarts the time-to-live, so the item outlives the time-to-live of its insertion.
    ManualClock::advance(60ms);
    constraint.on_update(key, TestItem{4, 42, 4}, TestItem{4, 43, 4});
    ManualClock::advance(60ms);
    EXPECT_FALSE(constraint.is_expired(key));

    // The cache hit restarts it again, so the item outlives the time-to-live of the update.
    constraint.on_cache_hit(key, TestItem{4, 43, 4});
    ManualClock::advance(100ms);
    EXPECT_FALSE(constraint.is_expired(key));

    ManualClock::advance(1ms);
    EXPECT_TRUE(constraint.is_expired(key));
}

TEST(ConstraintTimeToLive, LongestTimeToLiveNeverExpires)
{
    TestConstraint constraint{ManualClock::duration::max()};
    const uint32_t key = 1;

    // Insert after the clock's epoch, so the expiration time is past the clock's range.
    ManualClock::advance(1ms);
    constraint.on_insert(key, TestItem{4, 42, 4});

    ManualClock::advance(1ms);
    EXPECT_FALSE(constraint.is_expired(key));
    EXPECT_EQ(constraint.next_expired(), nullptr);
}

TEST(ConstraintTimeToLive, NextExpiredIsTheLeastRecentlyUsedExpiredKey)
{
    TestConstraint constraint{100ms};
    const uint32_t first  = 1;
    const uint32_t second = 2;
    constraint.on_insert(first, TestItem{4, 1, 4});
    ManualClock::advance(50ms);
    constraint.on_insert(second, TestItem{4, 2, 4});

    ManualClock::advance(50ms);
    EXPECT_EQ(constraint.next_expired(), nullptr);

    // The hit makes the first key the most recently used, so the second one expires first.
    constraint.on_cache_hit(first, TestItem{4, 1, 4});
    ManualClock::advance(51ms);
    EXPECT_EQ(constraint.next_expired(), &second);

    constraint.on_evict(second, TestItem{4, 2, 4});
    EXPECT_EQ(constraint.next_expired(), nullptr);
}

TEST(ConstraintTimeToLive, CopiesTrackTheirOwnItems)
{
    TestConstraint original{100ms};
    const uint32_t first  = 1;
    const uint32_t second = 2;
    original.on_insert(first, TestItem{4, 1, 4});
    ManualClock::advance(50ms);
    original.on_insert(second, TestItem{4, 2, 4});

    // The cache copies its constraint to try out evictions, so the copies must not share the original's state.
    TestConstraint copy{original};
    TestConstraint assigned{1ms};
    assigned = original;

    copy.on_cache_hit(first, TestItem{4, 1, 4});
    copy.on_evict(second, TestItem{4, 2, 4});
    assigned.on_evict(first, TestItem{4, 1, 4});

    ManualClock::advance(51ms);
    EXPECT_EQ(copy.next_expired(), nullptr);
    EXPECT_EQ(assigned.next_expired(), nullptr);
    EXPECT_EQ(original.next_expired(), &first);
    EXPECT_FALSE(original.is_expired(second));
}

TEST(ConstraintTimeToLive, NeverLimitsSize)
{
    TestConstraint constraint{100ms};

    EXPECT_TRUE(constraint.can_add(1, TestItem{4, 42, 1'000'000}));
    EXPECT_TRUE(constraint.can_replace(1, TestItem{4, 42, 1'000'000}, TestItem{4, 42, 2'000'000}));
    EXPECT_TRUE(constraint.is_satisfied());
}

TEST(TimeToLiveCache, UsableWithoutBind)
{
    Cache<uint32_t,
          uint32_t,
          policy::InsertionAlways,
          policy::EvictionLRU,
          policy::ConstraintTimeToLive,
          measurement::SizeOf<uint32_t>,
          measurement::SizeOf<uint32_t>>
        cache{std::chrono::minutes{1}};

    ASSERT_TRUE(cache.insert(1, 42));
    EXPECT_EQ(cache.find(1).value_or(0), 42u);
}

TEST(TimeToLiveCache, NothingIsEvictedBeforeItExpires)
{
    TestCache      cache{100ms};
    const uint32_t number_of_items = 1000;
    for (uint32_t key = 0; key < number_of_items; ++key) {
        ASSERT_TRUE(cache.insert(key, key));
    }
    EXPECT_EQ(cache.number_of_items(), number_of_items);
}

TEST(TimeToLiveCache, InsertionRemovesExpiredItems)
{
    TestCache cache{100ms};
    ASSERT_TRUE(cache.insert(1, 1));
    ASSERT_TRUE(cache.insert(2, 2));
    ManualClock::advance(50ms);
    ASSERT_TRUE(cache.insert(3, 3));
    ManualClock::advance(51ms);

    // 1 and 2 expired, 3 didn't.
    ASSERT_TRUE(cache.insert(4, 4));
    EXPECT_EQ(cache.number_of_items(), 2u);
    EXPECT_FALSE(cache.contains(1));
    EXPECT_FALSE(cache.contains(2));
    EXPECT_TRUE(cache.contains(3));
    EXPECT_TRUE(cache.contains(4));
}

TEST(TimeToLiveCache, InsertingAnExpiredKeyInsertsItAgain)
{
    TestCache cache{100ms};
    ASSERT_TRUE(cache.insert(1, 1));
    ManualClock::advance(101ms);

    ASSERT_TRUE(cache.insert(1, 2));
    EXPECT_EQ(cache.number_of_items(), 1u);
    EXPECT_EQ(cache.find(1).value_or(0), 2u);
}

TEST(TimeToLiveCache, CacheHitsExtendTimeToLive)
{
    TestCache cache{100ms};
    ASSERT_TRUE(cache.insert(1, 42));

    // Each hit restarts the time-to-live, so 1 survives an insertion 120ms after it was inserted.
    ManualClock::advance(60ms);
    EXPECT_TRUE(cache.find(1).has_value());
    ManualClock::advance(60ms);
    ASSERT_TRUE(cache.insert(2, 2));
    EXPECT_TRUE(cache.contains(1));

    // 1 was last hit 101ms ago and 2 was inserted 41ms ago.
    ManualClock::advance(41ms);
    ASSERT_TRUE(cache.insert(3, 3));
    EXPECT_FALSE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
}

TEST(TimeToLiveCache, RemoveExpiredRemovesOnlyExpiredItems)
{
    TestCache cache{100ms};
    ASSERT_TRUE(cache.insert(1, 1));
    ManualClock::advance(50ms);
    ASSERT_TRUE(cache.insert(2, 2));
    ManualClock::advance(51ms);

    // Without this call, find would still return the expired item 1.
    cache.remove_expired();
    EXPECT_FALSE(cache.find(1).has_value());
    EXPECT_EQ(cache.number_of_items(), 1u);
    EXPECT_TRUE(cache.contains(2));
}

TEST(TimeToLiveCache, ExpiredItemsAreRemovedWhateverTheEvictionOrder)
{
    Cache<uint32_t,
          uint32_t,
          policy::InsertionAlways,
          policy::EvictionSegmentedLRU,
          policy::bind<policy::BasicConstraintTimeToLive, ManualClock>::ttype,
          measurement::SizeOf<uint32_t>,
          measurement::SizeOf<uint32_t>>
        cache{100ms};
    cache.eviction_policy().set_protected_segment_size(1);

    // The hit moves 1 to the protected segment, so segmented LRU would evict 2 from the probation segment before 1.
    ASSERT_TRUE(cache.insert(1, 1));
    EXPECT_TRUE(cache.find(1).has_value());
    ManualClock::advance(50ms);
    ASSERT_TRUE(cache.insert(2, 2));
    ManualClock::advance(51ms);

    // 1 expired, 2 didn't.
    cache.remove_expired();
    EXPECT_FALSE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
}
