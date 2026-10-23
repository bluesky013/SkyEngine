#include <aurora/shader/ShaderUniform.h>

#include <aurora/rhi/ShaderParameters.h>

namespace sky::aurora {
    std::variant<std::string, std::vector<std::byte>> PackShaderUniform(const ShaderBlockLayout &block, std::span<const ShaderUniformValue> values)
    {
        std::vector<std::pair<std::string, ShaderDataValue>> typed;
        for (const auto &value : values) {
            ShaderDataType type = ShaderDataType::UNKNOWN;
            switch (value.type) {
            case RgFieldType::FLOAT: type = ShaderDataType::FLOAT; break;
            case RgFieldType::INT: type = ShaderDataType::INT; break;
            case RgFieldType::UINT: type = ShaderDataType::UINT; break;
            case RgFieldType::FLOAT2: type = ShaderDataType::FLOAT2; break;
            case RgFieldType::FLOAT4: type = ShaderDataType::FLOAT4; break;
            case RgFieldType::MAT4: type = ShaderDataType::MAT4; break;
            default: return std::string("Unsupported uniform property type");
            }
            typed.emplace_back(value.name, ShaderDataValue{.type = type, .bytes = {value.bytes.begin(), value.bytes.end()}});
        }
        std::vector<std::byte> bytes;
        std::string            error;
        if (!PackShaderData(block, typed, bytes, error))
            return error;
        return bytes;
    }
} // namespace sky::aurora
