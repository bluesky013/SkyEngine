#pragma once

#include "prepare/PrepareProducts.h"
#include "scene/CpuSceneStore.h"
#include <atomic>
#include <core/ecs/Dependency.h>
#include <cstddef>
#include <span>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sky::aurora {

    inline AccessOwner SceneAccessOwner(SceneId scene)
    {
        return {uint64_t{scene.index} + 1, scene.generation};
    }

    struct FrozenViewInput {
        CpuViewState view;
        Extent2D     outputExtent;
        PixelFormat  outputFormat   = PixelFormat::UNDEFINED;
        uint64_t     outputRevision = 0;
        uint64_t     sceneRevision  = 0;
    };

    struct PrepareChunkRange {
        size_t index = 0;
        size_t total = 1;
        size_t first = 0;
        size_t count = 0;
    };

    struct PrepareChunkStorage {
        PrepareChunkRange      range;
        std::vector<std::byte> scratch;
    };

    class FrozenPrepareExecutor;
    class FrozenPrepareContext;
    struct FrozenBindResult;

    class PrepareAccessScope {
    public:
        bool Check() const noexcept;
        bool Reject(std::string_view diagnostic) const noexcept;
        bool Failed() const noexcept
        {
            return failed.load(std::memory_order_acquire);
        }
        bool FailedWhileActive() const noexcept
        {
            return active.load(std::memory_order_acquire) && Failed();
        }

    private:
        friend class FrozenPrepareContext;
        friend class FrozenPrepareExecutor;
        std::atomic_bool         active = false;
        std::thread::id          owner;
        mutable std::atomic_bool failed = false;
        mutable std::string      diagnostic;
    };

    struct FrozenEntityRow {
        using Inputs = std::tuple<const RenderTransform *,
                                  const RenderMeshBinding *,
                                  const RenderMaterialBinding *,
                                  const RenderVisibility *,
                                  const RenderProperties *>;
        RenderEntityId    id;
        uint64_t          revision = 0;
        Inputs            inputs;
        RenderObjectData *object = nullptr;
        RenderObjectData  staged;
        bool              written = false;

        template <typename T>
        const T *Read(bool stagedObject) const
        {
            if constexpr (std::is_same_v<T, RenderObjectData>) {
                return stagedObject ? &staged : object;
            } else {
                return std::get<const T *>(inputs);
            }
        }
    };

    struct FrozenSceneBinding {
        SceneId                      scene;
        bool                         writeObject = false;
        std::vector<FrozenEntityRow> rows;
    };

    template <typename... Ts>
    class FrozenReadView {
    public:
        template <typename Fn>
        bool ForEach(Fn &&fn) const
        {
            if (!binding || (scope && ((requireActive && !scope->Check()) || scope->FailedWhileActive())))
                return false;
            for (size_t i = offset; i < offset + count; ++i) {
                const auto &row = binding->rows[i];
                fn(row.id, *row.Read<Ts>(binding->writeObject)...);
                if (scope && scope->FailedWhileActive())
                    return false;
            }
            return true;
        }
        explicit operator bool() const noexcept
        {
            return binding != nullptr;
        }
        size_t Size() const
        {
            return count;
        }
        FrozenReadView Slice(size_t first, size_t size) const
        {
            if (!binding || first > count || size > count - first) {
                if (scope)
                    scope->Reject("Frozen read range exceeds its parent view");
                return {};
            }
            auto view = *this;
            view.offset += first;
            view.count = size;
            return view;
        }

    private:
        friend class FrozenPrepareContext;
        std::shared_ptr<const FrozenCpuFrame>     frame;
        std::shared_ptr<const FrozenSceneBinding> binding;
        std::shared_ptr<const PrepareAccessScope> scope;
        size_t                                    offset        = 0;
        size_t                                    count         = 0;
        bool                                      requireActive = false;
    };

    template <typename T, typename... Reads>
    class FrozenWriteView {
        static_assert(std::is_same_v<T, RenderObjectData>, "Only registered derived components are writable");

    public:
        template <typename Fn>
        bool ForEach(Fn &&fn) const
        {
            return Visit(false, [](RenderEntityId, const T &) { return false; }, std::forward<Fn>(fn));
        }
        template <typename Fn>
        bool ForEachDirty(Fn &&fn) const
        {
            return Visit(true, [](RenderEntityId, const T &) { return false; }, std::forward<Fn>(fn));
        }
        explicit operator bool() const noexcept
        {
            return binding != nullptr;
        }
        size_t Size() const
        {
            return count;
        }
        FrozenWriteView Slice(size_t first, size_t size) const
        {
            if (!binding || first > count || size > count - first) {
                if (scope)
                    scope->Reject("Frozen write range exceeds its parent view");
                return {};
            }
            auto view = *this;
            view.offset += first;
            view.count = size;
            return view;
        }

        template <typename Predicate, typename Fn>
        bool ForEachInvalidated(Predicate &&invalidated, Fn &&fn) const
        {
            return Visit(true, std::forward<Predicate>(invalidated), std::forward<Fn>(fn));
        }

    private:
        friend class FrozenPrepareContext;
        template <typename Predicate, typename Fn>
        bool Visit(bool dirtyOnly, Predicate &&invalidated, Fn &&fn) const
        {
            if (!binding || !scope || !scope->Check())
                return false;
            for (size_t i = offset; i < offset + count; ++i) {
                auto &row = binding->rows[i];
                if (!dirtyOnly || !row.object->valid || row.object->sourceRevision != row.revision || invalidated(row.id, *row.object)) {
                    auto value = row.staged.value;
                    fn(row.id, value, *row.Read<Reads>(binding->writeObject)...);
                    if (scope->Failed())
                        return false;
                    row.staged.value = value;
                    row.staged.valid = false;
                    row.written      = true;
                }
            }
            return true;
        }
        std::shared_ptr<const FrozenCpuFrame>     frame;
        std::shared_ptr<FrozenSceneBinding>       binding;
        std::shared_ptr<const PrepareAccessScope> scope;
        size_t                                    offset = 0;
        size_t                                    count  = 0;
    };

    class FrozenCpuFrame : public std::enable_shared_from_this<FrozenCpuFrame> {
    public:
        FrozenCpuFrame(const FrozenCpuFrame &)            = delete;
        FrozenCpuFrame &operator=(const FrozenCpuFrame &) = delete;
        uint64_t        Sequence() const
        {
            return sequence;
        }
        std::span<const SceneId> Scenes() const
        {
            return selection.scenes;
        }
        std::span<const FrozenViewInput> Views() const
        {
            return views;
        }
        FrozenBindResult Bind(const SystemDesc &system) const;

    private:
        friend class CpuSceneStore;
        friend class FrozenPrepareExecutor;
        FrozenCpuFrame() = default;
        CpuSceneState               *FindScene(SceneId id) const;
        uint64_t                     sequence = 0;
        CpuPrepareSelection          selection;
        std::vector<FrozenViewInput> views;
        std::vector<CpuSceneState *> scenes;
        std::shared_ptr<void>        lease;
        mutable std::atomic_bool     prepareStarted = false;
    };

    // References cannot escape ForEach. Derived views work only on their kernel's thread, during that invocation.
    class FrozenPrepareContext {
    public:
        bool                             Fail(std::string_view diagnostic) const noexcept;
        bool                             Emit(uint64_t channel, std::span<const std::byte> bytes) const noexcept;
        bool                             Defer(DeferredEntityAction action) const noexcept;
        std::optional<PrepareChunkRange> Chunk() const;
        // Raw scratch spans, like component references, must not escape the current kernel invocation.
        std::span<std::byte> Scratch() const noexcept;
        SystemId             Id() const
        {
            return system;
        }
        uint64_t Sequence() const
        {
            return frame->Sequence();
        }
        std::span<const FrozenViewInput> Views() const
        {
            return frame->Views();
        }

        template <typename... Ts>
        FrozenReadView<Ts...> Read(SceneId scene) const
        {
            static_assert(sizeof...(Ts) != 0);
            if (!(Require(ComponentAccess<Ts>("scene", SceneAccessOwner(scene), AccessMode::READ)) && ...))
                return {};
            FrozenReadView<Ts...> view;
            view.frame   = frame;
            view.binding = FindBinding(scene);
            if (!view.binding) {
                scope->Reject("Missing frozen scene binding");
                return {};
            }
            view.count = view.binding->rows.size();
            view.scope = scope;
            if constexpr ((std::is_same_v<Ts, RenderObjectData> || ...)) {
                if (!scope->Check())
                    return {};
                view.requireActive = true;
            }
            return view;
        }

        template <typename T, typename... Reads>
        FrozenWriteView<T, Reads...> Write(SceneId scene) const
        {
            if (!Require(ComponentAccess<T>("scene", SceneAccessOwner(scene), AccessMode::WRITE)) ||
                !(Require(ComponentAccess<Reads>("scene", SceneAccessOwner(scene), AccessMode::READ)) && ...) || !scope->Check())
                return {};
            FrozenWriteView<T, Reads...> view;
            view.frame   = frame;
            view.binding = FindBinding(scene);
            if (!view.binding) {
                scope->Reject("Missing frozen scene binding");
                return {};
            }
            view.scope = scope;
            view.count = view.binding->rows.size();
            return view;
        }

    private:
        friend class FrozenCpuFrame;
        friend class FrozenPrepareExecutor;
        FrozenPrepareContext(std::shared_ptr<const FrozenCpuFrame> frame, const SystemDesc &desc)
            : frame(std::move(frame)), system(desc.id), accesses(desc.accesses), scope(std::make_shared<PrepareAccessScope>())
        {
        }
        bool                                             Require(const AccessDesc &access) const;
        std::shared_ptr<FrozenSceneBinding>              FindBinding(SceneId scene) const;
        void                                             Begin();
        void                                             End(bool success) noexcept;
        void                                             Publish() noexcept;
        std::shared_ptr<const FrozenCpuFrame>            frame;
        SystemId                                         system;
        std::vector<AccessDesc>                          accesses;
        std::vector<std::shared_ptr<FrozenSceneBinding>> bindings;
        std::shared_ptr<PrepareAccessScope>              scope;
        std::shared_ptr<PrepareChunkStorage>             chunk;
        std::shared_ptr<PrepareTaskProducts>             products;
    };

    struct FrozenBindResult {
        std::optional<FrozenPrepareContext> context;
        std::optional<RendererError>        error;
    };

} // namespace sky::aurora
