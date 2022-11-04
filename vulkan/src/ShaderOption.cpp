//
// Created by Zach Lee on 2022/1/13.
//

#include <algorithm>
#include <core/hash/Crc32.h>
#include <core/util/Memory.h>
#include <vulkan/ShaderOption.h>

namespace sky::vk {

    void ShaderOption::Builder::AddConstant(VkShaderStageFlagBits stage, uint32_t id, uint32_t size)
    {
        auto &map = constants[stage];
        map[id]   = Align(size, 4);
    }

    std::shared_ptr<ShaderOption> ShaderOption::Builder::Build()
    {
        auto                                       res   = std::make_shared<ShaderOption>();
        uint32_t                                   size  = 0;
        uint32_t                                   index = 0;
        std::vector<std::pair<uint32_t, uint32_t>> offsets;
        std::vector<std::pair<uint32_t, uint32_t>> sizes;
        for (auto &stage : constants) {
            res->specializationInfo.emplace_back(VkSpecializationInfo{0, nullptr, 0, nullptr});
            res->stages.emplace_back(stage.first);
            offsets.emplace_back(size, index);
            auto &info = res->specializationInfo.back();

            uint32_t currentOffset = 0;
            for (auto &id : stage.second) {
                res->entries.emplace_back(VkSpecializationMapEntry{id.first, currentOffset, id.second});
                currentOffset += id.second;
                size += id.second;
                index++;
            }
            sizes.emplace_back(size - offsets.back().first, index - offsets.back().second);
        }
        if (size != 0) {
            res->size    = size;
            res->storage = std::make_unique<uint8_t[]>(size);
        }

        for (uint32_t i = 0; i < res->specializationInfo.size(); ++i) {
            auto &info         = res->specializationInfo[i];
            info.mapEntryCount = sizes[i].second;
            info.pMapEntries   = &res->entries[offsets[i].second];
            info.dataSize      = sizes[i].first;
            info.pData         = res->storage.get() + offsets[i].first;
        }
        return res;
    }

    const VkSpecializationInfo *ShaderOption::GetSpecializationInfo(VkShaderStageFlagBits stage) const
    {
        auto iter = std::find(stages.begin(), stages.end(), stage);
        if (iter == stages.end()) {
            return nullptr;
        }
        auto distance = std::distance(stages.begin(), iter);
        return &specializationInfo[distance];
    }

    const uint8_t *ShaderOption::GetData() const
    {
        return storage.get();
    }

    uint32_t ShaderOption::GetHash() const
    {
        return hash;
    }

    void ShaderOption::CalculateHash()
    {
        if (storage != 0) {
            hash = Crc32::Cal(storage.get(), size);
        }
    }
} // namespace sky::vk
