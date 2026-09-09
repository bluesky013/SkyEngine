//
// Aurora render scene implementation.
//

#include <aurora/scene/RenderScene.h>

namespace sky::aurora {

    SceneView *RenderScene::CreateView(const Name &name)
    {
        auto [it, inserted] = mViews.try_emplace(name, std::make_unique<SceneView>());
        (void)inserted;
        return it->second.get();
    }

    SceneView *RenderScene::GetView(const Name &name) const
    {
        auto it = mViews.find(name);
        return it != mViews.end() ? it->second.get() : nullptr;
    }

} // namespace sky::aurora
