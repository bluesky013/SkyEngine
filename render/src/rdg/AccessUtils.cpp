//
// Created by Zach Lee on 2023/7/23.
//

#include <render/rdg/AccessUtils.h>

#include <unordered_map>
#include <render/rdg/RenderGraph.h>

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
    };

    const std::unordered_map<rhi::ShaderStageFlagBit, rhi::AccessFlagBit> CBVMap{
        {rhi::ShaderStageFlagBit::VS, rhi::AccessFlagBit::VERTEX_CBV},
        {rhi::ShaderStageFlagBit::FS, rhi::AccessFlagBit::FRAGMENT_CBV},
        {rhi::ShaderStageFlagBit::CS, rhi::AccessFlagBit::COMPUTE_CBV},
    };

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
            }}, deps.type);
        return res;
    }

    rhi::ImageLayout GetImageLayout(const DependencyInfo &deps) {
        rhi::ImageLayout layout = rhi::ImageLayout::UNDEFINED;

        std::visit(Overloaded{
        [&](const RasterType &type) {
            if (type & RasterTypeBit::INPUT) {
                if (deps.access & ResourceAccessBit::WRITE) {
                    layout = rhi::ImageLayout::FEEDBACK_LOOP;
                } else if (type & RasterTypeBit::COLOR) {
                    layout = rhi::ImageLayout::SHADER_READ_ONLY;
                } else if (type & RasterTypeBit::DEPTH_STENCIL) {
                    layout = rhi::ImageLayout::DEPTH_STENCIL_READ_ONLY;
                }
            } else if ((type & RasterTypeBit::COLOR) || (type & RasterTypeBit::RESOLVE)) {
                layout = rhi::ImageLayout::COLOR_ATTACHMENT;
            } else if (type & RasterTypeBit::DEPTH_STENCIL) {
                layout = rhi::ImageLayout::DEPTH_STENCIL_ATTACHMENT;
            } else if (type & RasterTypeBit::SHADING_RATE) {
                layout = rhi::ImageLayout::FRAGMENT_SHADING_RATE_ATTACHMENT;
            }
        },
        [&](const ComputeType &type) {
            if (type == ComputeType::SRV) { layout = rhi::ImageLayout::SHADER_READ_ONLY; }
            else if (type == ComputeType::UAV) { layout = rhi::ImageLayout::GENERAL; }
        },
        [&](const TransferType &type) {
            if (type == TransferType::SRC) { layout = rhi::ImageLayout::TRANSFER_SRC; }
            else if (type == TransferType::DST) { layout = rhi::ImageLayout::TRANSFER_DST; }
        }}, deps.type);
        return layout;
    }

    void MergeSubRange(AccessRange &result, const AccessRange &val)
    {
        result.aspectMask |= val.aspectMask;
        auto maxLevel = std::max(result.base + result.range, val.base + val.range);
        result.base = std::min(result.base, val.base);
        result.range = maxLevel - result.base;

        auto maxLayer = std::max(result.layer + result.layers, val.layer + val.layers);
        result.layer = std::min(result.layer, val.layer);
        result.layers = maxLayer - result.layer;
    }

    bool Intersection(const AccessRange &lhs, const AccessRange &rhs)
    {
        if (!(lhs.aspectMask & rhs.aspectMask)) {
            return false;
        }
        if ((lhs.layer > (rhs.layer + rhs.layers)) ||
            (rhs.layer > (lhs.layer + lhs.layers))) {
            return false;
        }
        if ((lhs.base > (rhs.base + rhs.range)) ||
            (rhs.base > (lhs.base + lhs.range))) {
            return false;
        }
        return true;
    }

} // namespace sky::rdg