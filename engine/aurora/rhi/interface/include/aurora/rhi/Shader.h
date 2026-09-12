//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/archive/BinaryData.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderReflection.h>

namespace sky::aurora {

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

    struct ShaderSpecializationEntry {
        uint32_t id    = 0;   // specialization constant id
        uint32_t value = 0;   // value (bool/int/uint; float via bit reinterpretation)
    };
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
            const ShaderReflection*    reflection     = nullptr; // whole-program layout
            const ShaderSpecialization* specialization = nullptr; // spec constant values
        };

        Shader() = default;
        ~Shader() override = default;

    protected:
        ShaderSpecialization specialization;
    };
    using ShaderPtr = CounterPtr<Shader>;

} // namespace sky::aurora