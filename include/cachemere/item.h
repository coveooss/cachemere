#ifndef CACHEMERE_ITEM_H
#define CACHEMERE_ITEM_H

#include <cstring>
#include <utility>

namespace cachemere {

/// @brief A wrapper for items stored in the cache.
template<typename Value> struct Item {
    Item(size_t key_size, Value value, size_t value_size)
     : m_key_size{key_size},
       m_value{std::move(value)},
       m_value_size{value_size},
       m_total_size{key_size + value_size}
    {
    }
    Item(Item&& other) noexcept = default;
    Item(const Item& other)     = delete;
    Item& operator=(const Item&) = delete;
    Item& operator=(Item&&) noexcept = default;

    size_t m_key_size;  //!< The size of the key.

    Value  m_value;       //!< The item stored in cache.
    size_t m_value_size;  //!< The size of the item.

    size_t m_total_size;  //!< The total size of the item (`m_key_size + m_value_size`)
};

template<typename Value> void swap(Item<Value>& a, Item<Value>& b)
{
    using std::swap;

    swap(a.m_key_size, b.m_key_size);
    swap(a.m_value, b.m_value);
    swap(a.m_value_size, b.m_value_size);
    swap(a.m_total_size, b.m_total_size);
}

}  // namespace cachemere

#endif
