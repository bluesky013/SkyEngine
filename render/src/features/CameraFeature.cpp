//
// Created by Zach Lee on 2022/7/18.
//

#include <render/features/CameraFeature.h>
#include <render/RenderScene.h>
#include <render/RenderConstants.h>
#include <render/RenderViewport.h>

namespace sky {

    RenderCamera* CameraFeature::Create()
    {
        cameras.emplace_back(new RenderCamera());
        auto camera = cameras.back().get();
        camera->Init();
        if (cameras.size() > MAX_ACTIVE_CAMERA) {
            camera->active = false;
        }
        return cameras.back().get();
    }

    void CameraFeature::Release(RenderCamera* camera)
    {
        cameras.erase(std::remove_if(cameras.begin(), cameras.end(),[camera](auto& ptr) {
            return ptr.get() == camera;
        }), cameras.end());
    }

    void CameraFeature::OnPreparePipeline()
    {
        auto viewBuffer = scene.GetMainViewBuffer();
        for (auto& camera : cameras) {
            if (camera->IsActive()) {
                camera->UpdateProjection();
                auto view = camera->GetView();
                view->SetViewTag(MAIN_CAMERA_TAG);
                if (view->IsDirty()) {
                    viewBuffer->Write(view->GetViewInfo());
                }
                scene.AddView(view);
            }
        }
    }

    void CameraFeature::OnViewportSizeChange(const RenderViewport& viewport)
    {
        auto& ext = viewport.GetExtent();
        for (auto& camera : cameras) {
            if (camera->AspectFromViewport()) {
                camera->SetAspect(static_cast<float>(ext.width) / static_cast<float>(ext.height));
            }
        }
    }
}