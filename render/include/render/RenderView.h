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

        void SetTransform(const Matrix4& transform);

        void SetProjectMatrix(const Matrix4& projectMatrix);

        const ViewInfo& GetViewInfo() const;

        void SetViewTag(uint32_t tag);

        uint32_t GetViewTag() const;

    private:
        ViewInfo viewInfo;
        uint32_t viewTag = 0;
    };
    using RDViewPtr = std::shared_ptr<RenderView>;

}