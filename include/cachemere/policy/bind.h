#ifndef CACHEMERE_POLICY_BIND_H
#define CACHEMERE_POLICY_BIND_H

namespace cachemere::policy {

/// @brief Binds a `Policy<K, V, Args...>` template to a `Policy<K, V>` template to be used with the cache.
template<template<typename...> class Policy, typename... Args> struct bind {
    template<typename K, typename V> using ttype = Policy<K, V, Args...>;
};

}  // namespace cachemere::policy

#endif
