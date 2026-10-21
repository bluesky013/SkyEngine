#pragma once

#include <aurora/rhi/ShaderReflection.h>
#include <aurora/shader/RgBlockDesc.h>
#include <cstddef>
#include <span>
#include <variant>

namespace sky::aurora {
    struct ShaderUniformValue {
        std::string                name;
        RgFieldType                type;
        std::span<const std::byte> bytes;
    };
    // One assembler supplies every field; reflection offsets never reuse CPU property offsets.
    std::variant<std::string, std::vector<std::byte>> PackShaderUniform(const ShaderBlockLayout &, std::span<const ShaderUniformValue>);
} // namespace sky::aurora
