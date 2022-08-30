//
// Created by Zach Lee on 2022/8/19.
//

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/PipelineLayout.h>
#include <vulkan/vulkan_core.h>

namespace sky::drv {

    class PushConstants {
    public:
        PushConstants()  = default;
        ~PushConstants() = default;

        class Builder {
        public:
            Builder(PushConstants &constants) : reference(constants)
            {
            }
            ~Builder() = default;

            Builder &AddRange(const VkPushConstantRange &range);

            void Finalize();

        private:
            PushConstants &reference;
            uint32_t       size = 0;
        };

        static std::shared_ptr<PushConstants> CreateFromPipelineLayout(const PipelineLayoutPtr &layout);

        template <typename T>
        void WriteData(const T &value, uint32_t offset = 0)
        {
            auto valueSize = sizeof(T);
            if (offset + valueSize > data.size()) {
                return;
            }
            uint8_t *ptr = data.data() + offset;
            memcpy(ptr, &value, sizeof(T));
        }

        void OnBind(VkCommandBuffer) const;

    private:
        friend class Builder;
        std::vector<uint8_t>             data;
        std::vector<VkPushConstantRange> ranges;
        PipelineLayoutPtr                pipelineLayout;
    };
    using PushConstantsPtr = std::shared_ptr<PushConstants>;

} // namespace sky::drv