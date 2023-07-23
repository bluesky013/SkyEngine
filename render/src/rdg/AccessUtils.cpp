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
        rhi::AccessFlag readAccess;
        rhi::AccessFlag writeAccess;
    };

    const std::unordered_map<AttachmentType, AccessFlagMask> AttachmentMask{
        {RasterTypeBit::COLOR, {0x3, 0x2, rhi::AccessFlag::COLOR_READ, rhi::AccessFlag::COLOR_WRITE}},
        {RasterTypeBit::RESOLVE, {0x3, 0x2, rhi::AccessFlag::COLOR_READ, rhi::AccessFlag::COLOR_WRITE}},
        {RasterTypeBit::INPUT, {0x1, 0x2, rhi::AccessFlag::COLOR_READ, rhi::AccessFlag::COLOR_WRITE}},
        {RasterTypeBit::DEPTH_STENCIL, {0x3, 0x2, rhi::AccessFlag::DEPTH_STENCIL_READ, rhi::AccessFlag::DEPTH_STENCIL_WRITE}},
        {RasterTypeBit::COLOR | RasterTypeBit::INPUT, {0x3, 0x2, rhi::AccessFlag::COLOR_INOUT_READ, rhi::AccessFlag::COLOR_INOUT_WRITE}},
        {RasterTypeBit::DEPTH_STENCIL | RasterTypeBit::INPUT,
         {0x3, 0x2, rhi::AccessFlag::DEPTH_STENCIL_INOUT_READ, rhi::AccessFlag::DEPTH_STENCIL_INOUT_WRITE}},
        {RasterTypeBit::SHADING_RATE, {0x1, 0x2, rhi::AccessFlag::SHADING_RATE, rhi::AccessFlag::NONE}},
        {ComputeType::CBV, {0x1, 0xFF}},
        {ComputeType::SRV, {0x1, 0xFF}},
        {ComputeType::UAV, {0x3, 0xFF}},
        {TransferType::SRC, {0x3, 0xFF}},
        {TransferType::DST, {0x3, 0xFF}},
    };

    const std::unordered_map<rhi::ShaderStageFlagBit, rhi::AccessFlag> CBVMap{
        {rhi::ShaderStageFlagBit::VS, rhi::AccessFlag::VERTEX_CBV},
        {rhi::ShaderStageFlagBit::FS, rhi::AccessFlag::FRAGMENT_CBV},
        {rhi::ShaderStageFlagBit::CS, rhi::AccessFlag::COMPUTE_CBV},
    };

    std::vector<rhi::AccessFlag> GetAccessFlag(const AccessEdge &edge)
    {
        SKY_ASSERT(AttachmentMask.count(edge.type));
        auto accessMask = AttachmentMask.at(edge.type);
        SKY_ASSERT((edge.access & ResourceAccess(accessMask.writeMask)) == edge.access);
        SKY_ASSERT((edge.visibility & rhi::ShaderStageFlags(accessMask.visibilityMask)) == edge.visibility);

        std::vector<rhi::AccessFlag> res;
        std::visit(Overloaded{
            [&](const RasterType &type) {
                if (edge.access & ResourceAccessBit::READ) {
                    res.emplace_back(accessMask.readAccess);
                }
                if (edge.access & ResourceAccessBit::WRITE) {
                    res.emplace_back(accessMask.writeAccess);
                }
            },
            [&](const ComputeType &type) {
                auto fn = [&](rhi::ShaderStageFlagBit stage, ComputeType type) {
                    auto base = static_cast<uint32_t>(CBVMap.at(stage)) + static_cast<uint32_t>(type);
                    if (edge.access & ResourceAccessBit::READ) {
                        res.emplace_back(static_cast<rhi::AccessFlag>(base));
                    }
                    if (edge.access & ResourceAccessBit::WRITE) {
                        res.emplace_back(static_cast<rhi::AccessFlag>(base + 1));
                    }
                };

                if (edge.visibility & rhi::ShaderStageFlagBit::VS) {
                    fn(rhi::ShaderStageFlagBit::VS, type);
                }
                if (edge.visibility & rhi::ShaderStageFlagBit::FS) {
                    fn(rhi::ShaderStageFlagBit::FS, type);
                }
                if (edge.visibility & rhi::ShaderStageFlagBit::CS) {
                    fn(rhi::ShaderStageFlagBit::CS, type);
                }
            },
            [&](const TransferType &type) {
                res.emplace_back(type == TransferType::SRC ? rhi::AccessFlag::TRANSFER_READ : rhi::AccessFlag::TRANSFER_WRITE);
            }}, edge.type);
        return res;
    }

    rhi::ImageLayout GetImageLayout(const AccessEdge &edge) {
        rhi::ImageLayout layout = rhi::ImageLayout::UNDEFINED;

        std::visit(Overloaded{
        [&](const RasterType &type) {
            if (type & RasterTypeBit::INPUT) {
                if (edge.access & ResourceAccessBit::WRITE) {
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
        }}, edge.type);
        return layout;
    }

} // namespace sky::rdg