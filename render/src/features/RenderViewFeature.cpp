//
// Created by Zach Lee on 2022/7/18.
//

#include <render/features/RenderViewFeature.h>
#include <render/RenderScene.h>

namespace sky {

    RDViewPtr RenderViewFeature::CreateView()
    {
        auto view = std::make_shared<RenderView>();
        views.emplace_back(view);
        return view;
    }

    void RenderViewFeature::RemoveView(RDViewPtr view)
    {
        auto iter = std::find_if(views.begin(), views.end(), [&view](RDViewPtr& v) {
            return view.get() == v.get();
        });
        if (iter != views.end()) {
            views.erase(iter);
        }
    }

    void RenderViewFeature::OnPrepareView(RenderScene& scene)
    {
        for (auto& view : views) {
            scene.AddView(view);
        }
    }

    void RenderViewFeature::OnRender(RenderScene& scene)
    {

    }

    void RenderViewFeature::OnPostRender(RenderScene& scene)
    {

    }

}