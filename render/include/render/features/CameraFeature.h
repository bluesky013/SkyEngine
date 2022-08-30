//
// Created by Zach Lee on 2022/7/18.
//

#pragma once

#include <core/util/Macros.h>
#include <render/RenderCamera.h>
#include <render/RenderFeature.h>

namespace sky {

    class CameraFeature : public RenderFeature {
    public:
        CameraFeature(RenderScene &scn) : RenderFeature(scn)
        {
        }
        ~CameraFeature() = default;

        SKY_DISABLE_COPY(CameraFeature)

        RenderCamera *Create();

        void OnViewportSizeChange(const RenderViewport &viewport) override;

        void Release(RenderCamera *camera);

        void OnPreparePipeline() override;

    private:
        std::vector<std::unique_ptr<RenderCamera>> cameras;
    };

} // namespace sky