//
// Created by blues on 2025/4/21.
//

#pragma once

#include <shader/node/ShaderDataType.h>
#include <span>
#include <string>
#include <vector>

namespace sky::sl {

    struct ResourceGroupDecl;
    struct ResourceDecl;

    class BufferLayoutCalculator {
    public:
        static uint32_t BaseTypeSize(ShaderBaseType baseType);

        static LayoutInfo CalculateType(const ValueType &type, LayoutStandard std);

        static uint32_t CalculateMembers(
            std::span<const MemberDecl> members,
            const ResourceGroupDecl *group,
            LayoutStandard std,
            std::vector<LayoutInfo> &outLayouts);

        static uint32_t CalculateStructSize(
            const StructDecl &decl,
            const ResourceGroupDecl *group = nullptr);

        struct ValidationMessage {
            enum class Level { WARNING, ERROR };
            Level       level;
            std::string message;
        };

        static std::vector<ValidationMessage> Validate(
            const ResourceDecl &cbuffer,
            const ResourceGroupDecl *group = nullptr);

    private:
        static uint32_t Align(uint32_t value, uint32_t alignment);

        static LayoutInfo CalculateStructLayout(
            const StructDecl &decl,
            const ResourceGroupDecl *group,
            LayoutStandard std);
    };

} // namespace sky::sl
