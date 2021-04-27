namespace cachemere::policy {

template<typename Key, typename Value> bool InsertionAlways<Key, Value>::should_add(const Key& /* key */)
{
    return true;
}

template<typename Key, typename Value> bool InsertionAlways<Key, Value>::should_replace(const Key& /* key */, const Key& /* candidate */)
{
    return true;
}

}  // namespace cachemere::policy
