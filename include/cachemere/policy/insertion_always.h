#ifndef CACHEMERE_INSERTION_ALWAYS_H
#define CACHEMERE_INSERTION_ALWAYS_H

#include <iostream>

namespace cachemere::policy {

template<typename Key, typename Value> class InsertionAlways
{
public:
    bool should_add(const Key& key);
    bool should_replace(const Key& victim, const Key& candidate);

    [[nodiscard]] size_t memory_used() const noexcept;
};

}  // namespace cachemere::policy

#include "insertion_always.hpp"

#endif
