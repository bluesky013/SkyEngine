//
// Created by blues on 2025/4/21.
//

#include <shader/node/HLSLResourceDeclGen.h>
#include <shader/node/ShaderNode.h>
#include <shader/node/ResourceGroupDecl.h>

#include <algorithm>
#include <numeric>
#include <sstream>

namespace sky::sl {

    static const char *TEX_TYPE_MAP[] = {
        "Texture",
        "Texture1D",
        "Texture2D",
        "Texture3D",
        "TextureCube",
        "Texture2DArray",
        "TextureCubeArray",
    };

    static const char *BASE_TYPE_MAP[] = {
        "",       // NONE
        "bool",
        "uint",
        "int",
        "half",
        "float",
        "double",
    };

    static void EmitTypeName(std::stringstream &ss, const ValueType &type, std::string_view structRef)
    {
        if (type.dataType == ShaderDataType::STRUCT) {
            ss << structRef;
            return;
        }

        const char *base = BASE_TYPE_MAP[static_cast<uint8_t>(type.baseType)];

        switch (type.dataType) {
        case ShaderDataType::SCALAR:
            ss << base;
            break;
        case ShaderDataType::VECTOR:
            ss << base << static_cast<uint32_t>(type.row);
            break;
        case ShaderDataType::MATRIX:
            ss << base << static_cast<uint32_t>(type.row) << 'x' << static_cast<uint32_t>(type.column);
            break;
        default:
            ss << base;
            break;
        }
    }

    static void EmitStructDecl(std::stringstream &ss, const StructDecl &decl)
    {
        ss << "struct " << decl.name << "\n{\n";
        for (const auto &m : decl.members) {
            ss << "    ";
            EmitTypeName(ss, m.type, m.structRef);
            ss << ' ' << m.name;
            if (m.arraySize > 0) {
                ss << '[' << m.arraySize << ']';
            }
            ss << ";\n";
        }
        ss << "};\n\n";
    }

    // -- Register Allocator --

    struct RegisterInfo {
        uint32_t vkBinding;
        char     prefix;
        uint32_t regIndex;
    };

    struct RegisterAllocator {
        uint32_t nextB  = 0;
        uint32_t nextT  = 0;
        uint32_t nextS  = 0;
        uint32_t nextU  = 0;
        uint32_t nextVk = 0;

        RegisterInfo Allocate(const ResourceDecl &node)
        {
            RegisterInfo info = {};
            info.vkBinding = (node.binding != UINT32_MAX) ? node.binding : nextVk;
            nextVk = std::max(nextVk, info.vkBinding + 1);

            switch (node.type) {
            case ResourceType::CONSTANT_BUFFER:
                info.prefix = 'b';
                info.regIndex = nextB++;
                break;
            case ResourceType::STRUCTURED_BUFFER:
                if (node.readOnly) {
                    info.prefix = 't';
                    info.regIndex = nextT++;
                } else {
                    info.prefix = 'u';
                    info.regIndex = nextU++;
                }
                break;
            case ResourceType::TEXTURE:
                if (node.storage) {
                    info.prefix = 'u';
                    info.regIndex = nextU++;
                } else {
                    info.prefix = 't';
                    info.regIndex = nextT++;
                }
                break;
            case ResourceType::SAMPLER:
                info.prefix = 's';
                info.regIndex = nextS++;
                break;
            }

            return info;
        }
    };

    // -- Emit individual resource types --

    static void EmitVkBinding(std::stringstream &ss, uint32_t binding, uint32_t set)
    {
        ss << "[[vk::binding(" << binding << ", " << set << ")]]\n";
    }

    static void EmitConstantBuffer(std::stringstream &ss, const ResourceDecl &cb,
                                   RegisterInfo reg, uint32_t set)
    {
        EmitVkBinding(ss, reg.vkBinding, set);
        ss << "cbuffer " << cb.name
           << " : register(" << reg.prefix << reg.regIndex
           << ", space" << set << ")\n{\n";

        for (const auto &m : cb.members) {
            ss << "    ";
            EmitTypeName(ss, m.type, m.structRef);
            ss << ' ' << m.name;
            if (m.arraySize > 0) {
                ss << '[' << m.arraySize << ']';
            }
            ss << ";\n";
        }
        ss << "}\n\n";
    }

    static void EmitStructuredBuffer(std::stringstream &ss, const ResourceDecl &sb,
                                     RegisterInfo reg, uint32_t set)
    {
        EmitVkBinding(ss, reg.vkBinding, set);

        if (sb.readOnly) {
            ss << "StructuredBuffer<";
        } else {
            ss << "RWStructuredBuffer<";
        }

        if (!sb.elementStructRef.empty()) {
            ss << sb.elementStructRef;
        } else {
            EmitTypeName(ss, sb.elementType, "");
        }

        ss << "> " << sb.name
           << " : register(" << reg.prefix << reg.regIndex
           << ", space" << set << ");\n\n";
    }

    static void EmitTexture(std::stringstream &ss, const ResourceDecl &tex,
                            RegisterInfo reg, uint32_t set)
    {
        EmitVkBinding(ss, reg.vkBinding, set);

        if (tex.storage) {
            ss << "RW";
        }
        ss << TEX_TYPE_MAP[static_cast<uint8_t>(tex.texType)];
        ss << ' ' << tex.name
           << " : register(" << reg.prefix << reg.regIndex
           << ", space" << set << ");\n\n";
    }

    static void EmitSampler(std::stringstream &ss, const ResourceDecl &smp,
                            RegisterInfo reg, uint32_t set)
    {
        EmitVkBinding(ss, reg.vkBinding, set);
        ss << "SamplerState " << smp.name
           << " : register(" << reg.prefix << reg.regIndex
           << ", space" << set << ");\n\n";
    }

    static void EmitResource(std::stringstream &ss, const ResourceDecl &res,
                             RegisterInfo reg, uint32_t set)
    {
        switch (res.type) {
        case ResourceType::CONSTANT_BUFFER:
            EmitConstantBuffer(ss, res, reg, set);
            break;
        case ResourceType::STRUCTURED_BUFFER:
            EmitStructuredBuffer(ss, res, reg, set);
            break;
        case ResourceType::TEXTURE:
            EmitTexture(ss, res, reg, set);
            break;
        case ResourceType::SAMPLER:
            EmitSampler(ss, res, reg, set);
            break;
        }
    }

    // -- Sort resources by binding --

    static std::vector<const ResourceDecl *> SortByBinding(
        std::span<const ResourceDecl> resources)
    {
        std::vector<const ResourceDecl *> sorted;
        sorted.reserve(resources.size());
        for (const auto &r : resources) {
            sorted.push_back(&r);
        }
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto *a, const auto *b) {
            uint32_t ba = (a->binding != UINT32_MAX) ? a->binding : UINT32_MAX;
            uint32_t bb = (b->binding != UINT32_MAX) ? b->binding : UINT32_MAX;
            return ba < bb;
        });
        return sorted;
    }

    // -- Main Generate --

    std::string HLSLResourceDeclGenerator::Generate(const ResourceGroupDecl &group)
    {
        std::stringstream ss;

        uint32_t set = group.setIndex;

        ss << "// AUTO-GENERATED by HLSLResourceDeclGenerator -- do not edit manually\n";
        ss << "// Source: ResourceGroupDecl \"" << group.name << "\" (set=" << set << ")\n\n";

        // Phase 1: Emit referenced struct declarations (dependency-ordered)
        auto referencedStructs = CollectReferencedStructs(group);
        for (const auto *decl : referencedStructs) {
            EmitStructDecl(ss, *decl);
        }

        // Phase 2: Emit unconditional resources
        RegisterAllocator alloc;

        auto sorted = SortByBinding(group.resources);
        for (const auto *res : sorted) {
            auto reg = alloc.Allocate(*res);
            EmitResource(ss, *res, reg, set);
        }

        // Phase 3: Emit conditional blocks
        for (const auto &cond : group.conditionals) {
            ss << "#if " << cond.condition << "\n\n";

            auto condSorted = SortByBinding(cond.resources);
            for (const auto *res : condSorted) {
                auto reg = alloc.Allocate(*res);
                EmitResource(ss, *res, reg, set);
            }

            ss << "#endif // " << cond.condition << "\n\n";
        }

        return ss.str();
    }

} // namespace sky::sl