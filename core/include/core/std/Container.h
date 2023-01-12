//
// Created by Zach Lee on 2023/1/12.
//

#pragma once

#include <vector>
#include <list>
#include <memory_resource>

namespace sky {

    using PmrUnSyncPoolRes = std::pmr::unsynchronized_pool_resource;
    using PmrSyncPoolRes = std::pmr::synchronized_pool_resource;
    using PmrMonoBufferRes = std::pmr::monotonic_buffer_resource;
    using PmrResource = std::pmr::memory_resource;

    template <typename T>
    using PmrVector = std::pmr::vector<T>;

    template <typename T>
    using PmrList = std::pmr::list<T>;
}
