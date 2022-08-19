//
// Created by Zach Lee on 2022/8/19.
//

#include <vulkan/PushConstants.h>

namespace sky::drv {

    void PushConstants::SetPipelineLayout(PipelineLayoutPtr layout)
    {
        pipelineLayout = layout;
    }

    void PushConstants::OnBind(VkCommandBuffer cmdBuffer)
    {
        uint8_t* ptr = data.data();
        for (auto& [stage, pair] : table) {
            vkCmdPushConstants(cmdBuffer, pipelineLayout->GetNativeHandle(), stage, 0, pair.second, ptr);
            ptr += pair.second;
        }
    }

    PushConstants::Builder& PushConstants::Builder::AddRange(const VkPushConstantRange& range)
    {
        return *this;
    }

    void PushConstants::Builder::Finalize()
    {
        reference.data.resize(size);
    }

}