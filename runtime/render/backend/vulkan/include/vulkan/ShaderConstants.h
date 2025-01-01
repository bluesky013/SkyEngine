//
// Created by Zach Lee on 2022/1/13.
//

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace sky::vk {

    class ShaderConstants {
    public:
        ShaderConstants()  = default;
        ~ShaderConstants() = default;

        class Builder {
        public:
            Builder()  = default;
            ~Builder() = default;

            void AddConstant(VkShaderStageFlagBits stage, uint32_t id, uint32_t size);

            std::shared_ptr<ShaderConstants> Build();

        private:
            using ConstantMap = std::map<uint32_t, uint32_t>;
            std::map<VkShaderStageFlagBits, ConstantMap> constants;
        };

        template <typename T>
        void SetConstant(VkShaderStageFlagBits stage, uint32_t id, const T &val)
        {
            auto iter = std::find(stages.begin(), stages.end(), stage);
            if (iter == stages.end()) {
                return;
            }
            auto &info = specializationInfo[std::distance(stages.begin(), iter)];
            for (uint32_t i = 0; i < info.mapEntryCount; ++i) {
                auto &entry = info.pMapEntries[i];
                if (entry.constantID == id) {
                    uint8_t *ptr = const_cast<uint8_t *>(static_cast<const uint8_t *>(info.pData) + entry.offset);
                    new (ptr) T(val);
                    return;
                }
            }
        }

        const VkSpecializationInfo *GetSpecializationInfo(VkShaderStageFlagBits) const;

        const uint8_t *GetData() const;

        uint32_t GetHash() const;

    private:
        friend class ShaderConstants::Builder;
        void CalculateHash();

        std::unique_ptr<uint8_t[]>            storage;
        std::vector<VkShaderStageFlagBits>    stages;
        std::vector<uint32_t>                 offsets;
        std::vector<VkSpecializationMapEntry> entries;
        std::vector<VkSpecializationInfo>     specializationInfo;
        uint32_t                              size = 0;
        uint32_t                              hash = 0;
    };
    using ShaderOptionPtr = std::shared_ptr<ShaderConstants>;

} // namespace sky::vk
