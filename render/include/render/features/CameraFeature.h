//
// Created by Zach Lee on 2022/7/18.
//

#pragma once

#include <render/RenderFeature.h>
#include <render/RenderCamera.h>
#include <core/util/Macros.h>

namespace sky {

    class CameraFeature : public RenderFeature {
    public:
        CameraFeature() = default;
        ~CameraFeature() = default;

        SKY_DISABLE_COPY(CameraFeature)

        RenderCamera* Create();

        void Release(RenderCamera* camera);

        void OnPrepareView(RenderScene& scene) override;

        void OnRender(RenderScene& scene) override;

        void OnPostRender(RenderScene& scene) override;

    private:
        std::vector<RDCameraPtr> cameras;
    };

}