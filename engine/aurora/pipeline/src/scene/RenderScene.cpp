//
// Aurora render scene implementation.
//

#include <aurora/pipeline/scene/RenderScene.h>

#include <algorithm>

namespace sky::aurora {

    void RenderScene::AddPrimitive(RenderPrimitive *primitive)
    {
        if (primitive == nullptr) {
            return;
        }
        if (std::find(mPrimitives.begin(), mPrimitives.end(), primitive) == mPrimitives.end()) {
            mPrimitives.push_back(primitive);
        }
    }

    void RenderScene::RemovePrimitive(RenderPrimitive *primitive)
    {
        mPrimitives.erase(std::remove(mPrimitives.begin(), mPrimitives.end(), primitive), mPrimitives.end());
    }

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
