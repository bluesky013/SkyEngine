//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Core.h>
#include <variant>

namespace sky::aurora {

    class ShaderFunction : public RefObject {
    public:
        struct Descriptor {

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