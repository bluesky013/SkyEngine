//
// Created by Zach Lee on 2022/8/16.
//

#pragma once

#include <render/resources/Material.h>

namespace sky {

    class RenderMaterialProxy {
    public:
        RenderMaterialProxy() = default;
        ~RenderMaterialProxy() = default;

    private:
        RDMaterialPtr material;
    };

}