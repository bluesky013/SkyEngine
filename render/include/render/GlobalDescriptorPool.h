//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <core/environment/Singleton.h>
#include <render/resources/DescriptorPool.h>
#include <unordered_map>

namespace sky {

    class GlobalDescriptorPool : public Singleton<GlobalDescriptorPool> {
    public:
        RDDescriptorPoolPtr GetPool(drv::DescriptorSetLayoutPtr layout);

    private:
        friend class Singleton<GlobalDescriptorPool>;
        GlobalDescriptorPool()  = default;
        ~GlobalDescriptorPool() = default;
        std::mutex                                        mutex;
        std::unordered_map<uint32_t, RDDescriptorPoolPtr> pools;
    };
} // namespace sky