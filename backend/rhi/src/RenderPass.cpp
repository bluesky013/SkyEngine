//
// Created by Zach Lee on 2023/5/20.
//

#include <rhi/RenderPass.h>

namespace sky::rhi {

    void RenderPass::InitInputMap(const Descriptor &desc)
    {
        subPassNum = static_cast<uint32_t>(desc.subPasses.size());
        subpassOutputMaps.resize(desc.subPasses.size());
        subpassInputMaps.resize(desc.subPasses.size());

        attachmentMap.resize(desc.attachments.size(), INVALID_INDEX);

        for (uint32_t i = 0; i < desc.subPasses.size(); ++i) {
            auto &sub = desc.subPasses[i];
            auto &outputMap = subpassOutputMaps[i];
            outputMap.resize(8, 0xFF);

            uint32_t outputOffset = static_cast<uint32_t>(sub.colors.size());
            auto &inputMap = subpassInputMaps[i];

            uint32_t drawBufferIndex = 0;
            for (auto &color : sub.colors) {
                auto &index = attachmentMap[color.index];
                if (index == INVALID_INDEX) {
                    index = static_cast<uint32_t>(colors.size());
                    colors.emplace_back(color.index);
                }
                outputMap[drawBufferIndex++] = index;
            }

            for (auto &resolve : sub.resolves) {
                auto &index = attachmentMap[resolve.index];
                if (index == INVALID_INDEX) {
                    index = static_cast<uint32_t>(resolves.size());
                    resolves.emplace_back(resolve.index);
                }
            }

            if (sub.depthStencil.index != INVALID_INDEX) {
                depthStencil = sub.depthStencil.index;
            }

            if (sub.dsResolve.index != INVALID_INDEX) {
                dsResolve = sub.dsResolve.index;
            }

            for (auto &input : sub.inputs) {
                if (input.index == depthStencil) {
                    if (input.mask & AspectFlagBit::DEPTH_BIT) {
                        inputMap.emplace_back(0xFD);
                    }
                    if (input.mask & AspectFlagBit::STENCIL_BIT) {
                        inputMap.emplace_back(0xFE);
                    }
                    continue;
                }

                auto iter = std::find_if(sub.colors.begin(), sub.colors.end(), [input](const auto &ref) {
                    return input.index == ref.index;
                });
                if (iter == sub.colors.end()) {
                    outputMap[drawBufferIndex++] = attachmentMap[input.index];
                }
                inputMap.emplace_back(iter == sub.colors.end() ? outputOffset++ : static_cast<uint32_t>(std::distance(sub.colors.begin(), iter)));
            }
        }
    }

}