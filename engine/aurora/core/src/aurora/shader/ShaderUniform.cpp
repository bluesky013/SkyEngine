#include <aurora/shader/ShaderUniform.h>

#include <algorithm>
#include <cstring>
#include <set>

namespace sky::aurora {
    std::variant<std::string, std::vector<std::byte>> PackShaderUniform(const ShaderBlockLayout &block, std::span<const ShaderUniformValue> values)
    {
        if ((block.size == 0u) || block.size > 65536 || values.size() != block.members.size()) {
            return std::string("Uniform assembler requires every declared field within the block limit");
        }
        std::vector<std::byte> bytes(block.size);
        std::vector<bool>      occupied(block.size);
        std::set<std::string>  names;
        for (const auto &value : values) {
            const auto member = std::ranges::find_if(block.members, [&](const auto &field) { return field.name == value.name; });
            if (!names.insert(value.name).second || member == block.members.end()) {
                return std::string("Duplicate or unknown uniform field");
            }
            ShaderScalarType scalar = ShaderScalarType::FLOAT;
            ShaderTypeKind   kind   = ShaderTypeKind::SCALAR;
            uint32_t         size   = 4;
            uint32_t         rows   = 1;
            uint32_t         cols   = 1;
            switch (value.type) {
            case RgFieldType::FLOAT: break;
            case RgFieldType::FLOAT2:
                cols = 2;
                kind = ShaderTypeKind::VECTOR;
                size = 8;
                break;
            case RgFieldType::FLOAT3:
                cols = 3;
                kind = ShaderTypeKind::VECTOR;
                size = 12;
                break;
            case RgFieldType::FLOAT4:
                cols = 4;
                kind = ShaderTypeKind::VECTOR;
                size = 16;
                break;
            case RgFieldType::MAT4:
                rows = cols = 4;
                kind        = ShaderTypeKind::MATRIX;
                size        = 64;
                break;
            case RgFieldType::INT: scalar = ShaderScalarType::INT; break;
            case RgFieldType::UINT: scalar = ShaderScalarType::UINT; break;
            default: return std::string("Unsupported uniform property type");
            }
            if (value.bytes.size() != size || member->size != size || member->scalarType != scalar || member->kind != kind || member->rows != rows ||
                member->cols != cols || (member->arrayCount != 0u) || member->offset > block.size || size > block.size - member->offset) {
                return std::string("Uniform value type or size disagrees with the reflected field");
            }
            for (uint32_t i = member->offset; i < member->offset + size; ++i) {
                if (occupied[i]) {
                    return std::string("Uniform fields overlap");
                }
                occupied[i] = true;
            }
            std::memcpy(bytes.data() + member->offset, value.bytes.data(), size);
        }
        return bytes;
    }
} // namespace sky::aurora
