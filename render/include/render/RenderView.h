//
// Created by Zach Lee on 2021/12/1.
//


#pragma once

#include <render/resources/Shader.h>
#include <core/math/Matrix.h>
#include <core/math/Vector.h>
#include <string>
#include <memory>

namespace sky {

    struct ViewInfo {
        Matrix4 viewToWorldMatrix;
        Matrix4 worldToViewMatrix;
        Matrix4 viewToClipMatrix;
        Matrix4 worldToClipMatrix;
        Vector3 position;
    };

    class RenderView {
    public:
        RenderView() = default;
        ~RenderView() = default;

        void UpdateData();

    private:
    };
    using RDViewPtr = std::shared_ptr<RenderView>;

}