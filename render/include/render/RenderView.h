//
// Created by Zach Lee on 2021/12/1.
//


#pragma once

#include <render/resources/Shader.h>
#include <core/math/Matrix.h>
#include <core/math/Vector.h>
#include <string>
#include <memory>
#include <vector>

namespace sky {
    class RenderPrimitive;

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

        void AddRenderPrimitive(RenderPrimitive* primitive);

        void Reset();

    private:
        ViewInfo viewInfo;
        uint32_t viewTag = 0;
        std::vector<RenderPrimitive*> primitives;
    };
    using RDViewPtr = std::shared_ptr<RenderView>;

}