//
// Created by blues on 2025/4/3.
//

#pragma once

#include <cstdint>
#include <string>

#include <rhi/Core.h>

namespace sky::sl {

    class ResourceGroupDecl;

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