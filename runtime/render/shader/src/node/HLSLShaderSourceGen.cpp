//
// Created by blues on 2025/4/21.
//

#include <shader/node/HLSLShaderSourceGen.h>
#include <shader/node/ShaderNode.h>

#include <sstream>

namespace sky::sl {

    static const char* TEX_TYPE_MAP[] = {
        "Texture",
        "Texture1D",
        "Texture2D",
        "Texture3D",
        "TextureCube",
        "Texture2DArray",
        "TextureCubeArray",
    };

    static const char* OPAQUE_TYPE_MAP[] = {
        "VOID",
        "bool",
        "int",
        "uint",
        "half",
        "float",
        "double",
    };

    static void GenSpace(std::stringstream &ss)
    {
        ss << ' ';
    }

    static void GenTab(std::stringstream &ss)
    {
        ss << "    ";
    }

    static void GenSemicolon(std::stringstream &ss)
    {
        ss << ';';
    }

    static void GenVkBinding(std::stringstream &ss, uint32_t binding, uint32_t set)
    {
        ss << "[vk::binding(" << binding << ", " << set << ")]\n";
    }

    static void GenHlslBinding(std::stringstream &ss, const std::string_view& name, const char* prefix, uint32_t reg, uint32_t space)
    {
        ss << "cbuffer " << name << "    : register(" << prefix << reg << ", space" << space << ")\n";
    }

    static void GenArray(std::stringstream& ss, uint8_t len)
    {
        ss << "[" << len << "]";
    }

//    static void GenOpaqueType(std::stringstream &ss, const Type& type)
//    {
//        ss << OPAQUE_TYPE_MAP[(uint8_t)(type.baseType)];
//        switch (type.dataType) {
//            case ShaderDataType::VECTOR:
//                ss << type.row;
//                break;
//            case ShaderDataType::MATRIX:
//                break;
//                ss << type.row << 'x' << type.column;
//            default:
//                break;
//        }
//    }

//    static void GenerateTypeDecl(std::stringstream &ss, VarDecl& var)
//    {
//        const auto& type = var.GetType();
//
//        GenTab(ss);
//        if (type.dataType <= ShaderDataType::MATRIX)
//        {
//            GenOpaqueType(ss, type);
//        }
//        GenSpace(ss);
//
//
//
//        GenSemicolon(ss);
//    }

//    std::string HLSLShaderGenerator::Generate(const ResourceGroupDecl& resGroup, uint32_t groupID)
//    {
//        std::string result;
//
//        uint32_t vkIndex = 0;
//
//        uint32_t bufferIndex = 0;
//        for (const auto& ubo : resGroup.GetConstantBuffers())
//        {
//            std::stringstream ss;
//            GenVkBinding(ss, vkIndex++, groupID);
//            GenHlslBinding(ss, ubo->GetName().GetStr(), "b", bufferIndex++, groupID);
//
//            ss << "{\n";
//
//            for (const auto& var : ubo->GetVars())
//            {
////                GenerateTypeDecl(ss, var);
//            }
//            ss << "}\n";
//        }
//
//        for (const auto& sbo : resGroup.GetStructuredBuffers())
//        {
//
//
//        }
//
//        for (const auto& tex : resGroup.GetTextures())
//        {
//
//
//        }
//
//        for (const auto& sampler : resGroup.GetSamplers())
//        {
//
//
//        }
//        return result;
//    }

} // namespace sky::sl