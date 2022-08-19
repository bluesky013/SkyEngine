//
// Created by Zach Lee on 2022/8/19.
//

#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <vulkan/vulkan_core.h>
#include <vulkan/PipelineLayout.h>

namespace sky::drv {

    class PushConstants {
    public:
        PushConstants() = default;
        ~PushConstants() = default;

        class Builder {
        public:
            Builder(PushConstants& constants) : reference(constants) {}
            ~Builder() = default;

            Builder& AddRange(const VkPushConstantRange& range);

            void Finalize();

        private:
            PushConstants& reference;
            uint32_t size = 0;
        };

        void SetPipelineLayout(PipelineLayoutPtr layout);

        template <typename T>
        void WriteData(VkShaderStageFlagBits stage, const T& value, uint32_t offset = 0)
        {
            auto iter = table.find(stage);
            if (iter == table.end()) {
                return;
            }
            uint32_t realOff = offset + iter->second.first;
            if (offset + sizeof(T) > iter->second.second) {
                return;
            }
            uint8_t* ptr = data.data() + realOff;
            memcpy(ptr, &value, sizeof(T));
        }

        void OnBind(VkCommandBuffer);

    private:
        friend class Builder;
        std::vector<uint8_t> data;
        std::unordered_map<VkShaderStageFlagBits, std::pair<uint32_t, uint32_t>> table; // offset, size
        PipelineLayoutPtr pipelineLayout;
    };
    using PushConstantsPtr = std::shared_ptr<PushConstants>;

}