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

    private:
        friend class CameraFeature;
        RenderCamera() = default;

        void Init();

        bool active = true;
        RDViewPtr renderView;
    };
    using RDCameraPtr = std::unique_ptr<RenderCamera>;

}
