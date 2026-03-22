//
// Created by blues on 2025/4/3.
//

#pragma once

#include <cstdint>
#include <string>

#include <rhi/Core.h>

namespace sky::sl {

    struct ResourceGroupDecl;

    class ResourceDeclGenerator {
    public:
        ResourceDeclGenerator() = default;
        ~ResourceDeclGenerator() = default;

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual std::string Generate(const ResourceGroupDecl &resGroup) = 0;
        };

        std::string Generate(const ResourceGroupDecl &resGroup, ShaderLanguage language);
    };

} // namespace sky::sl