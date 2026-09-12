//
// ShaderCodeGen implementation: reflected block -> C++ mirror header.
//

#include <aurora/shader/gen/ShaderCodeGen.h>

#include <sstream>

namespace sky::aurora {

    namespace {
        struct CppField {
            const char *type;   // C++ type name
            uint32_t    align;  // std140 alignment
            RgFieldType rgType; // RgBlockDesc field type
        };

        bool MapField(const ShaderBlockMember &member, CppField &out, std::string &error)
        {
            switch (member.kind) {
            case ShaderTypeKind::SCALAR:
                switch (member.scalarType) {
                case ShaderScalarType::FLOAT: out = {"float", 4, RgFieldType::FLOAT}; return true;
                case ShaderScalarType::INT:   out = {"int32_t", 4, RgFieldType::INT};  return true;
                case ShaderScalarType::UINT:  out = {"uint32_t", 4, RgFieldType::UINT}; return true;
                case ShaderScalarType::BOOL:  out = {"uint32_t", 4, RgFieldType::BOOL}; return true;
                default: break;
                }
                break;
            case ShaderTypeKind::VECTOR:
                if (member.scalarType == ShaderScalarType::FLOAT) {
                    switch (member.cols) {
                    case 2: out = {"Vector2", 8, RgFieldType::FLOAT2}; return true;
                    case 3: out = {"Vector3", 16, RgFieldType::FLOAT3}; return true;
                    case 4: out = {"Vector4", 16, RgFieldType::FLOAT4}; return true;
                    default: break;
                    }
                }
                break;
            case ShaderTypeKind::MATRIX:
                if (member.scalarType == ShaderScalarType::FLOAT && member.rows == 4 &&
                    member.cols == 4) {
                    out = {"Matrix4", 16, RgFieldType::MAT4};
                    return true;
                }
                break;
            default:
                break;
            }
            error = "unsupported field type for member '" + member.name + "'";
            return false;
        }

        const char *FieldTypeName(RgFieldType type)
        {
            switch (type) {
            case RgFieldType::FLOAT:  return "FLOAT";
            case RgFieldType::FLOAT2: return "FLOAT2";
            case RgFieldType::FLOAT3: return "FLOAT3";
            case RgFieldType::FLOAT4: return "FLOAT4";
            case RgFieldType::MAT4:   return "MAT4";
            case RgFieldType::TEXTURE2D: return "TEXTURE2D";
            case RgFieldType::TEXTURE_CUBE: return "TEXTURE_CUBE";
            case RgFieldType::SAMPLER: return "SAMPLER";
            case RgFieldType::INT:    return "INT";
            case RgFieldType::UINT:   return "UINT";
            case RgFieldType::BOOL:   return "BOOL";
            }
            return "FLOAT";
        }
    } // namespace

    bool ShaderCodeGen::BuildBlockDesc(const ShaderBlockLayout &block,
                                       RgBlockDesc &out,
                                       std::string &error)
    {
        out.set       = block.set;
        out.binding   = block.binding;
        out.blockName = Name(block.structName.empty() ? block.name.c_str() : block.structName.c_str());
        out.kind      = RgBlockKind::CBUFFER;

        out.fields.clear();
        out.fields.reserve(block.members.size());
        for (const auto &member : block.members) {
            CppField field{};
            if (!MapField(member, field, error)) {
                return false;
            }
            out.fields.push_back({field.rgType, Name(member.name.c_str())});
        }
        return true;
    }

    bool ShaderCodeGen::GenerateCppHeader(const ShaderBlockLayout &block,
                                          std::string &out,
                                          std::string &error)
    {
        RgBlockDesc desc{};
        if (!BuildBlockDesc(block, desc, error)) {
            return false;
        }

        const std::string structName =
            block.structName.empty() ? block.name : block.structName;

        std::ostringstream ss;
        ss << "// generated: do not edit\n";
        ss << "#pragma once\n\n";
        ss << "#include <cstddef>\n";
        ss << "#include <cstdint>\n";
        ss << "#include <core/math/Matrix4.h>\n";
        ss << "#include <core/math/Vector2.h>\n";
        ss << "#include <aurora/shader/RgBlockDesc.h>\n\n";
        ss << "namespace sky::aurora::generated {\n\n";

        ss << "    struct " << structName << " {\n";
        for (const auto &member : block.members) {
            CppField field{};
            if (!MapField(member, field, error)) {
                return false;
            }
            ss << "        alignas(" << field.align << ") " << field.type << " "
               << member.name << ";\n";
        }
        ss << "    };\n\n";

        ss << "    static_assert(sizeof(" << structName << ") == " << block.size
           << ", \"" << structName << " size mismatch\");\n";
        for (const auto &member : block.members) {
            ss << "    static_assert(offsetof(" << structName << ", " << member.name << ") == "
               << member.offset << ", \"" << structName << "." << member.name
               << " offset mismatch\");\n";
        }
        ss << "\n";

        const std::string accessor = "Get" + structName + "BlockDesc";
        ss << "    inline const RgBlockDesc &" << accessor << "()\n";
        ss << "    {\n";
        ss << "        static const RgBlockDesc desc = [] {\n";
        ss << "            RgBlockDesc d{};\n";
        ss << "            d.set       = " << desc.set << ";\n";
        ss << "            d.binding   = " << desc.binding << ";\n";
        ss << "            d.blockName = Name(\"" << desc.blockName.GetStr() << "\");\n";
        ss << "            d.kind      = RgBlockKind::CBUFFER;\n";
        ss << "            d.fields    = {\n";
        for (const auto &field : desc.fields) {
            ss << "                { RgFieldType::" << FieldTypeName(field.type) << ", Name(\""
               << field.name.GetStr() << "\") },\n";
        }
        ss << "            };\n";
        ss << "            return d;\n";
        ss << "        }();\n";
        ss << "        return desc;\n";
        ss << "    }\n\n";

        ss << "} // namespace sky::aurora::generated\n";

        out = ss.str();
        return true;
    }

} // namespace sky::aurora
