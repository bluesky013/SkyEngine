//
// Created by Zach Lee on 2022/7/18.
//

#include <render/features/CameraFeature.h>
#include <render/RenderScene.h>
#include <render/RenderConstants.h>

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
                auto view = camera->GetView();
                view->SetViewTag(MAIN_CAMERA_TAG);
                scene.AddView(view);

                viewBuffer->Write(view->GetViewInfo());
            }
        }
    }

}