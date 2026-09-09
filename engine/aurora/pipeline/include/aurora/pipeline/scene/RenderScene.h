//
// Aurora render scene: primitives + views registry. v1: no occlusion systems.
//

#pragma once

#include <core/name/Name.h>
#include <aurora/pipeline/scene/RenderPrimitive.h>
#include <aurora/pipeline/scene/SceneView.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace sky::aurora {

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        RenderScene(const RenderScene &) = delete;
        RenderScene &operator=(const RenderScene &) = delete;

        void AddPrimitive(RenderPrimitive *primitive);
        void RemovePrimitive(RenderPrimitive *primitive);

        const std::vector<RenderPrimitive *> &GetPrimitives() const { return mPrimitives; }

        SceneView *CreateView(const Name &name);
        SceneView *GetView(const Name &name) const;

    private:
        std::vector<RenderPrimitive *> mPrimitives;
        std::unordered_map<Name, std::unique_ptr<SceneView>> mViews;
    };

} // namespace sky::aurora
