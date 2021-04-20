#include <gtest/gtest.h>

#include <cstring>
#include <functional>

#include "cachemere/policy/detail/hash_mixer.h"

using namespace cachemere::policy::detail;

using TestMixer = HashMixer<std::string, std::hash<std::string>>;

TEST(HashMixer, StaysWithinRange)
{
    constexpr size_t upper_bound = 5;

    TestMixer mixer{"hello", upper_bound};

    for (auto i = 0; i < 10; ++i) {
        const size_t value = mixer();

        EXPECT_GE(value, 0);
        EXPECT_LE(value, upper_bound);
    }
}

TEST(HashMixer, StaysWithinRangeLong)
{
    constexpr size_t upper_bound = 500;

    TestMixer mixer{"hello", upper_bound};

    for (auto i = 0; i < 10000; ++i) {
        const size_t value = mixer();

        EXPECT_GE(value, 0);
        EXPECT_LE(value, upper_bound);
    }
}

TEST(HashMixer, DeterministicWithSameSeed)
{
    constexpr size_t run_length = 100;

    TestMixer mixer_a{"hello", 500};
    TestMixer mixer_b{mixer_a};

    for (auto i = 0; i < run_length; ++i) {
        EXPECT_EQ(mixer_a(), mixer_b());
    }
}
