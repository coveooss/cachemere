namespace cachemere::policy {

template<typename Key, typename Value> bool InsertionAlways<Key, Value>::should_add(const Key& key)
{
    return true;
}

template<typename Key, typename Value> bool InsertionAlways<Key, Value>::should_replace(const Key& victim, const Key& candidate)
{
    return true;
}

template<typename Key, typename Value> size_t InsertionAlways<Key, Value>::memory_used() const noexcept
{
    return 0;
}

}  // namespace cachemere::policy
