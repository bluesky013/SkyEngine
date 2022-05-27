//
// Created by Zach Lee on 2022/5/7.
//


#pragma once

#include <engine/render/resources/RenderResource.h>

namespace sky {

    class Material : public RenderResource {
    public:
        Material() = default;
        ~Material() = default;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;
}