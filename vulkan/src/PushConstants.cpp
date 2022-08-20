//
// Created by Zach Lee on 2022/8/19.
//

#include <vulkan/PushConstants.h>

namespace sky::drv {

    void PushConstants::OnBind(VkCommandBuffer cmdBuffer) const
    {
        const uint8_t* ptr = data.data();
        for (auto& range : ranges) {
            vkCmdPushConstants(cmdBuffer, pipelineLayout->GetNativeHandle(), range.stageFlags, range.offset, range.size, ptr + range.offset);
        }
    }

    std::shared_ptr<PushConstants> PushConstants::CreateFromPipelineLayout(const PipelineLayoutPtr& layout)
    {
        auto constants = std::make_shared<PushConstants>();
        constants->pipelineLayout = layout;

        auto& ranges = layout->GetConstantRanges();

        PushConstants::Builder builder(*constants);
        for (auto &range : ranges) {
            builder.AddRange(range);
        }
        builder.Finalize();
        return constants;
    }

    PushConstants::Builder& PushConstants::Builder::AddRange(const VkPushConstantRange& range)
    {
        size = std::max(range.size + range.offset, size);
        reference.ranges.emplace_back(range);
        return *this;
    }

    void PushConstants::Builder::Finalize()
    {
        reference.data.resize(size);
    }

}