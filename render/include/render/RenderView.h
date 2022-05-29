//
// Created by Zach Lee on 2021/12/1.
//


#pragma once

#include <core/math/Matrix.h>
#include <core/math/Vector.h>
#include <string>
#include <memory>

namespace sky {

    class RenderView {
    public:
        RenderView() = default;
        ~RenderView() = default;

        void SetTransform(const Matrix4& transform);

        void SetProjectMatrix(const Matrix4& projectMatrix);

        void UpdateData();

    private:
        Vector3 position;
        Matrix4 viewToWorldMatrix;
        Matrix4 worldToViewMatrix;
        Matrix4 viewToClipMatrix;
        Matrix4 worldToClipMatrix;
    };
    using RDViewPtr = std::shared_ptr<RenderView>;

}