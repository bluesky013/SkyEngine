//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/archive/BinaryData.h>
#include <aurora/rhi/Core.h>
#include <variant>

namespace sky::aurora {

    struct BufferNameHandler {
        uint32_t binding;
        uint32_t offset;
        uint32_t size;
    };

    struct BindingHandler {
        uint32_t binding;
        uint32_t size;
    };

    struct ShaderVertexInput {
    };

    struct ShaderDataProvider : RefObject {
        ShaderDataProvider() = default;
        ~ShaderDataProvider() override = default;
    };

    struct ShaderBinaryProvider : ShaderDataProvider {
        BinaryDataPtr binaryData;
    };

    class ShaderFunction : public RefObject {
    public:
        struct Descriptor {
            ShaderStageFlagBit stage;
            CounterPtr<ShaderDataProvider> data;
        };
        ShaderFunction() = default;
        ~ShaderFunction() override = default;
    };
    using ShaderFunctionPtr = CounterPtr<ShaderFunction>;

    using ShaderSpecializationEntry = std::variant<uint32_t, int32_t>;
    struct ShaderSpecialization {
        std::vector<ShaderSpecializationEntry> entries;
    };

    class Shader : public RefObject {
    public:
        struct Descriptor {
            union {
                struct {
                    ShaderFunction* vs;
                    ShaderFunction* ps;
                };
                struct {
                    ShaderFunction* cs;
                };
            };
        };

        Shader() = default;
        ~Shader() override = default;

    protected:
        ShaderSpecialization specialization;
    };
    using ShaderPtr = CounterPtr<Shader>;

} // namespace sky::aurora