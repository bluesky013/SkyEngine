//
// Created by Zach Lee on 2022/7/18.
//

#include <render/features/CameraFeature.h>
#include <render/RenderScene.h>

namespace sky {

    RenderCamera* CameraFeature::Create()
    {
        cameras.emplace_back(new RenderCamera());
        auto camera = cameras.back().get();
        camera->Init();
        // [CONSTANTS]
        if (cameras.size() > 1) {
            camera->active = false;
        }
        return cameras.back().get();
    }

    void CameraFeature::Release(RenderCamera* camera)
    {
        auto iter = std::find_if(cameras.begin(), cameras.end(), [&camera](RDCameraPtr & v) {
            return camera == v.get();
        });
        if (iter != cameras.end()) {
            cameras.erase(iter);
        }
    }

    void CameraFeature::OnPrepareView(RenderScene& scene)
    {
        for (auto& camera : cameras) {
            if (camera->IsActive()) {
                scene.AddView(camera->GetView());
            }
        }
    }

    void CameraFeature::OnRender(RenderScene& scene)
    {

    }

    void CameraFeature::OnPostRender(RenderScene& scene)
    {

    }

}