#pragma once

#include <cstdint>
#include <mutex>

namespace cachemere {

/// @brief The locking strategy to use for the cache. This determines the type of mutex used for synchronization.
enum class LockingStrategy : uint8_t { None, Mutex, TimedMutex };

namespace detail {

struct NullMutex {
    void lock() noexcept
    {
    }

    [[nodiscard]] bool try_lock() noexcept
    {
        return true;
    }

    void unlock() noexcept
    {
    }
};

template<LockingStrategy Strategy> struct MutexFor;

template<> struct MutexFor<LockingStrategy::None> {
    using type = NullMutex;
};

template<> struct MutexFor<LockingStrategy::Mutex> {
    using type = std::recursive_mutex;
};

template<> struct MutexFor<LockingStrategy::TimedMutex> {
    using type = std::recursive_timed_mutex;
};

template<LockingStrategy Strategy> using MutexForT                        = typename MutexFor<Strategy>::type;
template<LockingStrategy Strategy> inline constexpr bool HasTimedLockingV = Strategy == LockingStrategy::TimedMutex;

}  // namespace detail

}  // namespace cachemere
