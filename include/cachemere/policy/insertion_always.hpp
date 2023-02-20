namespace cachemere::policy {

template<typename Key, typename KeyHash, typename Value> void InsertionAlways<Key, KeyHash, Value>::clear()
{
}

template<typename Key, typename KeyHash, typename Value> bool InsertionAlways<Key, KeyHash, Value>::should_add(const Key& /* key */)
{
    return true;
}

template<typename Key, typename KeyHash, typename Value>
bool InsertionAlways<Key, KeyHash, Value>::should_replace(const Key& /* key */, const Key& /* candidate */)
{
    return true;
}

}  // namespace cachemere::policy
