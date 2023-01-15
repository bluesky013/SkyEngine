//
// Created by Zach Lee on 2022/5/29.
//

#include <algorithm>
#include <iterator>
#include <render/RHIManager.h>
#include <render/resources/DescriptorPool.h>

namespace sky {

    DescriptorPool *DescriptorPool::CreatePool(vk::DescriptorSetLayoutPtr layout, const Descriptor &descriptor)
    {
        auto pool    = new DescriptorPool(descriptor);
        pool->layout = layout;

        auto                                                      &desTable = layout->GetDescriptorTable();
        std::unordered_map<VkDescriptorType, VkDescriptorPoolSize> sizesTable;
        std::vector<VkDescriptorPoolSize>                         &sizes = pool->sizes;
        for (const auto &layoutBinding : desTable) {
            sizesTable[layoutBinding.second.descriptorType].descriptorCount += layoutBinding.second.descriptorCount * descriptor.maxSet;
        }
        sizes.reserve(sizesTable.size());
        std::transform(sizesTable.begin(), sizesTable.end(), std::back_inserter(sizes), [](auto &it) {
            it.second.type = it.first;
            return it.second;
        });
        return pool;
    }

    RDDesGroupPtr DescriptorPool::Allocate()
    {
        vk::DescriptorSetPtr result;
        for (uint32_t i = 0; i < pools.size(); ++i) {
            uint32_t ringIndex = (index + i) % pools.size();
            auto    &drvPool   = pools[ringIndex];
            result             = vk::DescriptorSet::Allocate(drvPool, layout);
            if (result) {
                index = ringIndex;
                break;
            }
        }
        if (!result) {
            pools.emplace_back(CreateInternal());
            index  = static_cast<uint32_t>(pools.size()) - 1;
            result = vk::DescriptorSet::Allocate(pools.back(), layout);
        }
        if (!result) {
            return {};
        }

        auto group = std::make_shared<DescriptorGroup>();
        group->set = result;
        group->Init();
        return group;
    }

    vk::DescriptorSetPoolPtr DescriptorPool::CreateInternal()
    {
        vk::DescriptorSetPool::VkDescriptor poolDesc = {};
        poolDesc.maxSets                            = descriptor.maxSet;
        poolDesc.num                                = static_cast<uint32_t>(sizes.size());
        poolDesc.sizes                              = sizes.data();

        auto pool = RHIManager::Get()->GetDevice()->CreateDeviceObject<vk::DescriptorSetPool>(poolDesc);
        return pool;
    }

} // namespace sky
