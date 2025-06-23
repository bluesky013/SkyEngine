//
// Created by Zach Lee on 2023/5/20.
//

#include <rhi/RenderPass.h>
#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <algorithm>

namespace sky::rhi {

    void RenderPass::CalculateHash(const Descriptor &desc)
    {
        hash = 0;
        for (const auto &attachment : desc.attachments) {
            HashCombine32(compatibleHash, static_cast<uint32_t>(attachment.format));
            HashCombine32(compatibleHash, static_cast<uint32_t>(attachment.sample));
        }
        for (const auto &subPass : desc.subPasses) {
            for (const auto &color : subPass.colors) {
                HashCombine32(compatibleHash, color.index);
            }
            for (const auto &resolve : subPass.resolves) {
                HashCombine32(compatibleHash, resolve.index);
            }
            for (const auto &input : subPass.inputs) {
                HashCombine32(compatibleHash, input.index);
            }
            HashCombine32(compatibleHash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.preserves.data()),
                                                     static_cast<uint32_t>(subPass.preserves.size()) * sizeof(uint32_t)));
            HashCombine32(compatibleHash, subPass.depthStencil.index);
            HashCombine32(compatibleHash, subPass.dsResolve.index);
            HashCombine32(compatibleHash, subPass.viewMask);
        }
        for (const auto &dep : desc.dependencies) {
            HashCombine32(compatibleHash, dep.src);
            HashCombine32(compatibleHash, dep.dst);
        }
        HashCombine32(compatibleHash, Crc32::Cal(reinterpret_cast<const uint8_t *>(desc.correlatedViewMasks.data()),
                                                 static_cast<uint32_t>(desc.correlatedViewMasks.size()) * sizeof(uint32_t)));
    }

    void RenderPass::InitInputMap(const Descriptor &desc)
    {
        subPassNum = static_cast<uint32_t>(desc.subPasses.size());
        subpassOutputMaps.resize(desc.subPasses.size());
        subpassInputMaps.resize(desc.subPasses.size());

        attachmentMap.resize(desc.attachments.size(), INVALID_INDEX);

        for (uint32_t i = 0; i < desc.subPasses.size(); ++i) {
            const auto &sub = desc.subPasses[i];
            auto &outputMap = subpassOutputMaps[i];
            outputMap.resize(8, 0xFF);

            auto outputOffset = static_cast<uint32_t>(sub.colors.size());
            auto &inputMap = subpassInputMaps[i];

            uint32_t drawBufferIndex = 0;
            for (const auto &color : sub.colors) {
                auto &index = attachmentMap[color.index];
                if (index == INVALID_INDEX) {
                    index = static_cast<uint32_t>(colors.size());
                    colors.emplace_back(color.index);
                }
                outputMap[drawBufferIndex++] = index;
            }

            for (const auto &resolve : sub.resolves) {
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

            for (const auto &input : sub.inputs) {
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
        CalculateHash(desc);
    }

}
