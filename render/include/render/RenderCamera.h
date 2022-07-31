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
        RDViewPtr renderView;
        bool active = true;
    };
}
