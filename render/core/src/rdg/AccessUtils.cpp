//
// Created by Zach Lee on 2023/7/23.
//

#include <render/rdg/AccessUtils.h>

#include <rhi/Decode.h>
#include <render/rdg/RenderGraph.h>
#include <unordered_map>

namespace std {

    template <>
    struct hash<sky::rdg::AttachmentType> {
        size_t operator()(const sky::rdg::AttachmentType &type) const noexcept
        {
            return std::visit(sky::Overloaded{[](const auto &arg) {
                                  return static_cast<uint32_t>(arg);
                              }}, type);
        }
    };

    template <>
    struct equal_to<sky::rdg::AttachmentType> {
        bool operator()(const sky::rdg::AttachmentType &lhs, const sky::rdg::AttachmentType &rhs) const noexcept
        {
            return lhs == rhs;
        }
    };

} // namespace std

namespace sky::rdg {

    /**
         *  |            AttachmentType                 |      RW      |   VISIBILITY   |
         *  | RT RS IN DS SR | CBV SRV UAV | CPS  CPD   |   R    W     |   VS  FS  CS   |
         *  | X                                             X    X             X        | COLOR_READ | COLOR_WRITE
         *  |    X                                          X    X             X        | COLOR_READ | COLOR_WRITE
         *  |       X                                       X                  X        | COLOR_INPUT | DEPTH_STENCIL_INPUT
         *  |          X                                    X    X             X        | DEPTH_STENCIL_READ | DEPTH_STENCIL_WRITE
         *  | X     X                                       X    X             X        | COLOR_INOUT_READ | COLOR_INOUT_WRITE
         *  |       X  X                                    X    X             X        | DEPTH_STENCIL_INOUT_READ | DEPTH_STENCIL_INOUT_WRITE
         *  |             X                                 X                  X        | SHADING_RATE
         *  ----------------------------------------------------------------------------
         *  |                  X                            X              X   X   X    | XX_CBV
         *  |                      X                        X              X   X   X    | XX_SRV
         *  |                          X                    X    X         X   X   X    | XX_READ_UAV | XX_WRITE_UAV
         *  ----------------------------------------------------------------------------
         *  |                                X                                          | TRANSFER_READ
         *  |                                     X                                     | TRANSFER_WRITE
     */

    struct AccessFlagMask {
        uint32_t        writeMask;
        uint32_t        visibilityMask;
        rhi::AccessFlagBit readAccess;
        rhi::AccessFlagBit writeAccess;
    };

    const std::unordered_map<AttachmentType, AccessFlagMask> AttachmentMask{
        {RasterTypeBit::COLOR,                                {0x3, 0x2, rhi::AccessFlagBit::COLOR_READ, rhi::AccessFlagBit::COLOR_WRITE}},
        {RasterTypeBit::RESOLVE,                              {0x3, 0x2, rhi::AccessFlagBit::COLOR_READ, rhi::AccessFlagBit::COLOR_WRITE}},
        {RasterTypeBit::INPUT,                                {0x1, 0x2, rhi::AccessFlagBit::COLOR_READ, rhi::AccessFlagBit::COLOR_WRITE}},
        {RasterTypeBit::DEPTH_STENCIL,                        {0x3, 0x2, rhi::AccessFlagBit::DEPTH_STENCIL_READ, rhi::AccessFlagBit::DEPTH_STENCIL_WRITE}},
        {RasterTypeBit::COLOR | RasterTypeBit::INPUT,         {0x3, 0x2, rhi::AccessFlagBit::COLOR_INOUT_READ, rhi::AccessFlagBit::COLOR_INOUT_WRITE}},
        {RasterTypeBit::DEPTH_STENCIL | RasterTypeBit::INPUT, {0x3, 0x2, rhi::AccessFlagBit::DEPTH_STENCIL_INOUT_READ, rhi::AccessFlagBit::DEPTH_STENCIL_INOUT_WRITE}},
        {RasterTypeBit::SHADING_RATE,                         {0x1, 0x2, rhi::AccessFlagBit::SHADING_RATE, rhi::AccessFlagBit::NONE}},
        {ComputeType::CBV,                                    {0x1, 0xFF}},
        {ComputeType::SRV,                                    {0x1, 0xFF}},
        {ComputeType::UAV,                                    {0x3, 0xFF}},
        {TransferType::SRC,                                   {0x3, 0xFF}},
        {TransferType::DST,                                   {0x3, 0xFF}},
        {PresentType::PRESENT,                                {0x3, 0xFF}}
    };

    const std::unordered_map<rhi::ShaderStageFlagBit, rhi::AccessFlagBit> CBVMap{
        {rhi::ShaderStageFlagBit::VS, rhi::AccessFlagBit::VERTEX_CBV},
        {rhi::ShaderStageFlagBit::FS, rhi::AccessFlagBit::FRAGMENT_CBV},
        {rhi::ShaderStageFlagBit::CS, rhi::AccessFlagBit::COMPUTE_CBV},
    };

    rhi::AccessFlags GetImportAccessFlags(const RenderGraph &graph, VertexType resID)
    {
        const auto &resourceGraph = graph.resourceGraph;
        rhi::AccessFlags flags = rhi::AccessFlagBit::NONE;

        std::visit(Overloaded{
            [&](const ImportImageTag &tag) {
                const auto &image = resourceGraph.importImages[Index(resID, resourceGraph)];
                flags = image.desc.importFlags;
            },
            [&](const ImportBufferTag &tag) {
                const auto &buffer = resourceGraph.importBuffers[Index(resID, resourceGraph)];
                flags = buffer.desc.importFlags;
            },
            [&](const auto &) {}
        }, rdg::Tag(resID, resourceGraph));
        return flags;
    }

    AccessRange GetAccessRange(const RenderGraph &graph, VertexType resID)
    {
        const auto &resourceGraph = graph.resourceGraph;
        AccessRange range = {};

        std::visit(Overloaded{
            [&](const ImageTag &tag) {
                const auto &image = resourceGraph.images[Index(resID, resourceGraph)];
                range = {
                    0, image.desc.mipLevels,
                    0, image.desc.arrayLayers
                };
            },
            [&](const ImportImageTag &tag) {
                const auto &image = resourceGraph.importImages[Index(resID, resourceGraph)];
                const auto &desc = image.desc.image->GetDescriptor();
                range = {
                    0, desc.mipLevels,
                    0, desc.arrayLayers
                };
            },
            [&](const ImageViewTag &tag) {
                const auto &view = resourceGraph.imageViews[Index(resID, resourceGraph)];
                range = {
                    view.desc.view.subRange.baseLevel, view.desc.view.subRange.levels,
                    view.desc.view.subRange.baseLayer, view.desc.view.subRange.layers
                };
            },
            [&](const ImportSwapChainTag &tag) {
                range.range = 1;
                range.layers = 1;
            },
            [&](const ImportXRSwapChainTag &tag) {
                const auto &res = resourceGraph.xrSwapChains[Index(resID, resourceGraph)];
                range.range = 1;
                range.layers = res.desc.swapchain->GetArrayLayers();
            },
            [&](const BufferTag &tag) {
                const auto &buffer = resourceGraph.buffers[Index(resID, resourceGraph)];
                range.range = buffer.desc.size;
            },
            [&](const ImportBufferTag &tag) {
                const auto &buffer = resourceGraph.importBuffers[Index(resID, resourceGraph)];
                const auto &desc = buffer.desc.buffer->GetBufferDesc();
                range.range = desc.size;
            },
            [&](const BufferViewTag &tag) {
                const auto &view = resourceGraph.bufferViews[Index(resID, resourceGraph)];
                range.base = view.desc.view.offset;
                range.range = view.desc.view.range;
            },
            [&](const auto &) {}
        }, rdg::Tag(resID, resourceGraph));
        return range;
    }

    rhi::AccessFlags GetAccessFlags(const DependencyInfo &deps)
    {
        SKY_ASSERT(AttachmentMask.count(deps.type));
        auto accessMask = AttachmentMask.at(deps.type);
        SKY_ASSERT((deps.access & ResourceAccess(accessMask.writeMask)) == deps.access);
        SKY_ASSERT((deps.visibility & rhi::ShaderStageFlags(accessMask.visibilityMask)) == deps.visibility);

        rhi::AccessFlags res;
        std::visit(Overloaded{
            [&](const RasterType &type) {
                if (deps.access & ResourceAccessBit::READ) {
                    res |= accessMask.readAccess;
                }
                if (deps.access & ResourceAccessBit::WRITE) {
                    res |= accessMask.writeAccess;
                }
            },
            [&](const ComputeType &type) {
                auto fn = [&](rhi::ShaderStageFlagBit stage, ComputeType type) {
                    auto base = static_cast<uint32_t>(CBVMap.at(stage));
                    if (deps.access & ResourceAccessBit::READ) {
                        res |= static_cast<rhi::AccessFlags>(base << static_cast<uint32_t>(type));
                    }
                    if (deps.access & ResourceAccessBit::WRITE) {
                        res |= static_cast<rhi::AccessFlags>(base << (static_cast<uint32_t>(type) + 1));
                    }
                };

                if (deps.visibility & rhi::ShaderStageFlagBit::VS) {
                    fn(rhi::ShaderStageFlagBit::VS, type);
                }
                if (deps.visibility & rhi::ShaderStageFlagBit::FS) {
                    fn(rhi::ShaderStageFlagBit::FS, type);
                }
                if (deps.visibility & rhi::ShaderStageFlagBit::CS) {
                    fn(rhi::ShaderStageFlagBit::CS, type);
                }
            },
            [&](const TransferType &type) {
                res = type == TransferType::SRC ? rhi::AccessFlagBit::TRANSFER_READ : rhi::AccessFlagBit::TRANSFER_WRITE;
            },
            [&](const PresentType &type) {
                res = rhi::AccessFlagBit::PRESENT;
            },
        }, deps.type);
        return res;
    }

    void MergeSubRange(AccessRange &result, const AccessRange &val)
    {
        auto maxLevel = std::max(result.base + result.range, val.base + val.range);
        result.base = std::min(result.base, val.base);
        result.range = maxLevel - result.base;

        auto maxLayer = std::max(result.layer + result.layers, val.layer + val.layers);
        result.layer = std::min(result.layer, val.layer);
        result.layers = maxLayer - result.layer;
    }

} // namespace sky::rdg
