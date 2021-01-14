#ifndef CACHEMERE_ITEM_H
#define CACHEMERE_ITEM_H

#include <cstring>

namespace cachemere::detail {

template <typename Key, typename Value>
struct Item {
    Item(const Key& key, size_t key_size, const Value& value, size_t value_size)
        : m_key{key},
          m_key_size{key_size},
          m_value{value},
          m_value_size{value_size},
          m_total_size{key_size + value_size} {}
    Item(const Item& p_Other) = delete;

    Key m_key;
    size_t m_key_size;

    Value m_value;
    size_t m_value_size;

    size_t m_total_size;
};

}  // namespace cachemere::detail

#endif
