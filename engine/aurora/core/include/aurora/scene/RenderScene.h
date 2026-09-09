//
// Aurora render scene: ECS entity registry + scene views.
//

#pragma once

#include <core/name/Name.h>
#include <core/ecs/EntityRegistry.h>
#include <aurora/scene/SceneTypes.h>
#include <aurora/scene/SceneView.h>

#include <memory>
#include <unordered_map>

namespace sky::aurora {

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        RenderScene(const RenderScene &) = delete;
        RenderScene &operator=(const RenderScene &) = delete;

        // ---- entities ----
        EntityId CreateEntity() { return mRegistry.CreateEntity(); }
        void DestroyEntity(EntityId id) { mRegistry.DestroyEntity(id); }
        bool IsAlive(EntityId id) const { return mRegistry.IsAlive(id); }

        template <typename T>
        T &Add(EntityId id, T value) { return mRegistry.Add<T>(id, std::move(value)); }

        template <typename T>
        T *Get(EntityId id) { return mRegistry.Get<T>(id); }

        template <typename T>
        const T *Get(EntityId id) const { return mRegistry.Get<T>(id); }

        template <typename T>
        void Remove(EntityId id) { mRegistry.Remove<T>(id); }

        template <typename T>
        SparseSet<T> &Pool() { return mRegistry.Pool<T>(); }

        EntityRegistry &GetRegistry() { return mRegistry; }

        // ---- views ----
        SceneView *CreateView(const Name &name);
        SceneView *GetView(const Name &name) const;

    private:
        EntityRegistry mRegistry;
        std::unordered_map<Name, std::unique_ptr<SceneView>> mViews;
    };

} // namespace sky::aurora
