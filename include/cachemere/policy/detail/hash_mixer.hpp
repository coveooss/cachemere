namespace cachemere::policy::detail {

template<typename Key, typename KeyHash>
HashMixer<Key, KeyHash>::HashMixer(const Key& key, size_t value_range)
 : KeyHash{},
   m_rng{static_cast<std::minstd_rand::result_type>(KeyHash::operator()(key))},
   m_value_range{value_range}
{
}

template<typename Key, typename KeyHash> size_t HashMixer<Key, KeyHash>::operator()()
{
    return m_rng() % m_value_range;
}

}  // namespace cachemere::policy::detail
