//
// Created by blues on 2025/4/21.
//

#include <shader/node/BufferLayoutCalculator.h>
#include <shader/node/ShaderNode.h>
#include <shader/node/ResourceGroupDecl.h>
#include <algorithm>
#include <sstream>

namespace sky::sl {

    uint32_t BufferLayoutCalculator::CalculateMembers(
        std::span<const MemberDecl> members,
        const ResourceGroupDecl *group,
        LayoutStandard std,
        std::vector<LayoutInfo> &outLayouts)
    {
        std::span<const StructDecl> structs = group ? group->localStructDecls
                                                    : std::span<const StructDecl>{};

        uint32_t currentOffset = 0;
        outLayouts.reserve(members.size());

        for (const auto &m : members) {
            LayoutInfo elemLayout = MemberLayout(m, structs, std);

            currentOffset = AlignUp(currentOffset, elemLayout.alignment);
            elemLayout.offset = currentOffset;
            currentOffset += elemLayout.size;

            outLayouts.push_back(elemLayout);
        }

        return currentOffset;
    }

    uint32_t BufferLayoutCalculator::CalculateStructSize(
        const StructDecl &decl,
        const ResourceGroupDecl *group)
    {
        std::span<const StructDecl> structs = group ? group->localStructDecls
                                                    : std::span<const StructDecl>{};
        return sl::CalculateStructLayout(decl, structs).size;
    }

    std::vector<BufferLayoutCalculator::ValidationMessage> BufferLayoutCalculator::Validate(
        const ResourceDecl &cbuffer,
        const ResourceGroupDecl *group)
    {
        std::vector<ValidationMessage> messages;

        std::vector<LayoutInfo> layouts;
        uint32_t totalSize = CalculateMembers(cbuffer.members, group, LayoutStandard::STD140, layouts);

        for (size_t i = 0; i < cbuffer.members.size(); ++i) {
            const auto &m = cbuffer.members[i];

            // float3 / int3 / uint3 warning
            if (m.type.dataType == ShaderDataType::VECTOR && m.type.row == 3) {
                std::ostringstream oss;
                oss << "Member '" << m.name << "' uses a 3-component vector. "
                    << "In std140, this occupies 16 bytes (aligned to vec4). "
                    << "Consider using a 4-component vector instead.";
                messages.push_back({ValidationMessage::Level::WARNING, oss.str()});
            }

            // Scalar array warning in std140
            if (m.arraySize > 0 && m.type.dataType == ShaderDataType::SCALAR) {
                std::ostringstream oss;
                oss << "Member '" << m.name << "' is a scalar array. "
                    << "In std140, each element occupies 16 bytes. "
                    << "Consider using a vec4 array instead.";
                messages.push_back({ValidationMessage::Level::WARNING, oss.str()});
            }
        }

        // UBO size limit warning (16KB mobile minimum)
        if (totalSize > 16384) {
            std::ostringstream oss;
            oss << "Constant buffer '" << cbuffer.name << "' total size is "
                << totalSize << " bytes, exceeding the 16KB mobile UBO limit.";
            messages.push_back({ValidationMessage::Level::WARNING, oss.str()});
        }

        return messages;
    }

} // namespace sky::sl
