//
// Created by Zach Lee on 2023/1/12.
//

#pragma once

#include <vector>
#include <list>
#include <unordered_map>

#ifndef WIN32
#define SKY_USE_BOOST
#endif

#ifdef SKY_USE_BOOST
    #include <boost/container/pmr/memory_resource.hpp>
    #include <boost/container/pmr/unsynchronized_pool_resource.hpp>
    #include <boost/container/pmr/vector.hpp>
    #include <boost/container/pmr/list.hpp>
    #include <boost/container/pmr/string.hpp>
    #include <boost/container/pmr/map.hpp>
#else
    #include <memory_resource>
#endif


namespace sky {

#ifdef SKY_USE_BOOST
    using PmrUnSyncPoolRes = boost::container::pmr::unsynchronized_pool_resource;
    using PmrSyncPoolRes = boost::container::pmr::synchronized_pool_resource;
    using PmrMonoBufferRes = boost::container::pmr::monotonic_buffer_resource;
    using PmrResource = boost::container::pmr::memory_resource;

    template <typename T>
    using PmrVector = boost::container::pmr::vector<T>;

    template <typename T>
    using PmrList = boost::container::pmr::list<T>;

    template <class K, class T, class Hash = std::hash<K>, class KeyEQ = std::equal_to<K>>
    using PmrHashMap = std::unordered_map<K, T, Hash, KeyEQ, boost::container::pmr::polymorphic_allocator<std::pair<const K, T>>>;

    using PmrString = boost::container::pmr::string;

#else
    using PmrUnSyncPoolRes = std::pmr::unsynchronized_pool_resource;
    using PmrSyncPoolRes = std::pmr::synchronized_pool_resource;
    using PmrMonoBufferRes = std::pmr::monotonic_buffer_resource;
    using PmrResource = std::pmr::memory_resource;

    template <typename T>
    using PmrVector = std::pmr::vector<T>;

    template <typename T>
    using PmrList = std::pmr::list<T>;

    template <class K, class T, class Hash = std::hash<K>, class KeyEQ = std::equal_to<K>>
    using PmrHashMap = std::pmr::unordered_map<K, T, Hash, KeyEQ>;

    using PmrString = std::pmr::string;
#endif

}
