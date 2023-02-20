#ifndef CACHEMERE_POLICY_BIND_H
#define CACHEMERE_POLICY_BIND_H

namespace cachemere::policy {

/// @brief Binds a `Policy<K, KH, V, Args...>` template to a `Policy<K, KH, V>` template to be used with the cache.
template<template<typename...> class Policy, typename... Args> struct bind {
    template<typename K, typename KH, typename V> using ttype = Policy<K, KH, V, Args...>;
};

}  // namespace cachemere::policy

#endif
