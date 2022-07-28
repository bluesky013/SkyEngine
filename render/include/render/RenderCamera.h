//
// Created by Zach Lee on 2022/7/28.
//

#pragma once
#include <memory>
#include <render/RenderView.h>

namespace sky {

    class RenderCamera {
    public:
        ~RenderCamera() = default;

        inline bool IsActive() const { return active; }

        inline RDViewPtr GetView() const { return renderView; }

        void SetTransform(const Matrix4& transform);

        void SetProjectMatrix(const Matrix4& projectMatrix);

    private:
        friend class CameraFeature;
        RenderCamera() = default;

        void Init();

        bool active = true;

        Vector3 position;
        Matrix4 viewToWorldMatrix;
        Matrix4 worldToViewMatrix;
        Matrix4 viewToClipMatrix;
        Matrix4 worldToClipMatrix;

        RDViewPtr renderView;
    };
}
