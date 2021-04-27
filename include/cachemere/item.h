#ifndef CACHEMERE_ITEM_H
#define CACHEMERE_ITEM_H

#include <cstring>

namespace cachemere {

/// @brief A wrapper for items stored in the cache.
template<typename Key, typename Value> struct Item {
    Item(Key key, size_t key_size, Value value, size_t value_size)
     : m_key{std::move(key)},
       m_key_size{key_size},
       m_value{std::move(value)},
       m_value_size{value_size},
       m_total_size{key_size + value_size}
    {
    }
    Item(const Item& p_Other) = delete;
    Item& operator=(const Item&) = delete;

    Key    m_key;       //!< The key of the item.
    size_t m_key_size;  //!< The size of the key.

    Value  m_value;       //!< The item stored in cache.
    size_t m_value_size;  //!< The size of the item.

    size_t m_total_size;  //!< The total size of the item (`m_key_size + m_value_size`)
};

}  // namespace cachemere

#endif
