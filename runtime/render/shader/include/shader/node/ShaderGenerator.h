//
// Created by blues on 2025/4/3.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::sl {

    class ResourceGroupDecl;

    enum class ShaderLanguage : uint8_t {
        HLSL,
        GLSL,
        MSL
    };

    class ShaderGenerator {
    public:
        ShaderGenerator() = default;
        ~ShaderGenerator() = default;

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual std::string Generate(const ResourceGroupDecl& resGroup, uint32_t groupID) = 0;
        };

        void GenerateByNode(ResourceGroupDecl* resGroup, uint32_t groupId, ShaderLanguage language);
    };

} // namespace sky::sl