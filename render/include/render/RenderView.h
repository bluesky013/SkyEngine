//
// Created by Zach Lee on 2021/12/1.
//


#pragma once

#include <render/resources/Shader.h>
#include <render/resources/DescirptorGroup.h>
#include <core/math/Matrix.h>
#include <core/math/Vector.h>
#include <string>
#include <memory>

namespace sky {

    class RenderView {
    public:
        RenderView() = default;
        ~RenderView() = default;

        void UpdateData();

    private:
        RDDesGroupPtr group;
    };
    using RDViewPtr = std::shared_ptr<RenderView>;

}