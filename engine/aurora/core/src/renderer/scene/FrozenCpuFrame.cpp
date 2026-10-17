#include "scene/FrozenCpuFrame.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::aurora {
    namespace {
        template <typename T>
        bool Matches(const AccessDesc &access)
        {
            return access.key.identity == TypeId<T>() && access.tag == TypeTag<T>();
        }
    } // namespace

    bool PrepareAccessScope::Check() const noexcept
    {
        if (!active.load(std::memory_order_acquire) || owner != std::this_thread::get_id()) {
            return Reject("Derived component access is outside its kernel invocation");
        }
        return !Failed();
    }
    bool PrepareAccessScope::Reject(std::string_view reason) const noexcept
    {
        if (active.load(std::memory_order_acquire)) {
            failed.store(true, std::memory_order_release);
            // A foreign thread only sets the flag; the invocation owner owns the diagnostic string.
            if (owner == std::this_thread::get_id())
                diagnostic = reason;
        }
        return false;
    }

    CpuSceneState *FrozenCpuFrame::FindScene(SceneId id) const
    {
        const auto it = std::find_if(scenes.begin(), scenes.end(), [&](const auto *scene) { return scene->id == id; });
        return it == scenes.end() ? nullptr : *it;
    }

    FrozenBindResult FrozenCpuFrame::Bind(const SystemDesc &system) const
    {
        FrozenPrepareContext context(shared_from_this(), system);
        for (const auto &access : system.accesses) {
            if (access.key.kind == AccessKind::RESOURCE) {
                continue;
            }
            const auto owner = access.key.owner;
            if (access.key.kind != AccessKind::COMPONENT || (access.mode != AccessMode::READ && access.mode != AccessMode::WRITE) ||
                access.key.domain != "scene" || owner.instance == 0 || owner.instance > std::numeric_limits<uint32_t>::max() ||
                owner.generation == 0 || owner.generation > std::numeric_limits<uint32_t>::max()) {
                return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Frozen components require a valid scene declaration"}};
            }
            const SceneId scene{static_cast<uint32_t>(owner.instance - 1), static_cast<uint32_t>(owner.generation)};
            auto         *state   = FindScene(scene);
            const bool    derived = Matches<RenderObjectData>(access);
            if (!state ||
                !(derived || Matches<RenderTransform>(access) || Matches<RenderMeshBinding>(access) || Matches<RenderMaterialBinding>(access) ||
                  Matches<RenderVisibility>(access) || Matches<RenderProperties>(access)) ||
                (access.mode == AccessMode::WRITE && !derived)) {
                return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Frozen component declaration does not resolve to permitted storage"}};
            }
            auto binding = context.FindBinding(scene);
            if (!binding) {
                binding        = std::make_shared<FrozenSceneBinding>();
                binding->scene = scene;
                binding->rows.reserve(state->entities.size());
                for (const auto &[index, entity] : state->entities) {
                    const auto &registry = state->registry;
                    auto  inputs = std::make_tuple(registry.Get<RenderTransform>(entity.entity), registry.Get<RenderMeshBinding>(entity.entity),
                                                   registry.Get<RenderMaterialBinding>(entity.entity), registry.Get<RenderVisibility>(entity.entity),
                                                   registry.Get<RenderProperties>(entity.entity));
                    auto *object = state->registry.Get<RenderObjectData>(entity.entity);
                    if (!object || !std::apply([](const auto *...values) { return ((values != nullptr) && ...); }, inputs)) {
                        return {{}, RendererError{RendererErrorCode::INTERNAL_ERROR, "Frozen scene is missing precreated components"}};
                    }
                    binding->rows.push_back({entity.id, entity.revision, inputs, object});
                }
                context.bindings.push_back(binding);
            }
            binding->writeObject |= derived && access.mode == AccessMode::WRITE;
        }
        return {std::move(context), {}};
    }

    bool FrozenPrepareContext::Require(const AccessDesc &access) const
    {
        const auto it = std::find_if(accesses.begin(), accesses.end(), [&](const auto &declared) {
            return declared.key == access.key && declared.tag == access.tag &&
                   (declared.mode == AccessMode::WRITE || access.mode == AccessMode::READ);
        });
        if (it == accesses.end()) {
            return scope->Reject("Undeclared frozen component access");
        }
        return true;
    }

    std::shared_ptr<FrozenSceneBinding> FrozenPrepareContext::FindBinding(SceneId scene) const
    {
        const auto it = std::find_if(bindings.begin(), bindings.end(), [&](const auto &binding) { return binding->scene == scene; });
        return it == bindings.end() ? nullptr : *it;
    }

    bool FrozenPrepareContext::Fail(std::string_view diagnostic) const noexcept
    {
        if (!scope->active.load(std::memory_order_acquire) || scope->owner != std::this_thread::get_id()) {
            return false;
        }
        scope->failed     = true;
        scope->diagnostic = diagnostic;
        return true;
    }

    bool FrozenPrepareContext::Emit(uint64_t channel, std::span<const std::byte> bytes) const noexcept
    {
        if (!scope->active.load(std::memory_order_acquire) || scope->owner != std::this_thread::get_id())
            return false;
        if (scope->failed)
            return false;
        if (!products || channel == 0 || products->data.records.size() >= products->limits.maxRecordsPerTask ||
            bytes.size() > products->limits.maxBytesPerTask - products->data.bytes.size()) {
            Fail("Invalid output channel or task output budget exceeded");
            return false;
        }
        auto      &data   = products->data;
        const auto offset = data.bytes.size();
        if (!bytes.empty())
            data.bytes.insert(data.bytes.end(), bytes.begin(), bytes.end());
        data.records.push_back({{system, chunk ? chunk->range.index : 0, data.records.size()}, channel, offset, bytes.size()});
        return true;
    }

    bool FrozenPrepareContext::Defer(DeferredEntityAction action) const noexcept
    {
        if (!scope->active.load(std::memory_order_acquire) || scope->owner != std::this_thread::get_id())
            return false;
        if (scope->failed)
            return false;
        if (!products || products->data.deferred.commands.size() >= products->limits.maxDeferredPerTask) {
            Fail("Task deferred command budget exceeded");
            return false;
        }
        std::optional<uint64_t> revision;
        const bool              valid = std::visit(
            [&](const auto &command) {
                using T            = std::decay_t<decltype(command)>;
                const auto binding = FindBinding(command.id.scene);
                if (!binding || !command.id.entity.IsValid())
                    return false;
                if constexpr (!std::is_same_v<T, DestroyRenderEntity>) {
                    if (!IsValidRenderEntityDesc(command.desc))
                        return false;
                }
                const auto row =
                    std::find_if(binding->rows.begin(), binding->rows.end(), [&](const auto &row) { return row.id == command.id.entity; });
                if constexpr (std::is_same_v<T, CreateRenderEntity>) {
                    return row == binding->rows.end();
                } else {
                    if (row == binding->rows.end())
                        return false;
                    revision = row->revision;
                    if constexpr (std::is_same_v<T, UpdateRenderEntity>)
                        return command.revision > *revision;
                    return true;
                }
            },
            action);
        if (!valid) {
            Fail("Deferred entity command is invalid or outside the task's bound rows");
            return false;
        }
        auto &commands = products->data.deferred.commands;
        commands.push_back({{system, chunk ? chunk->range.index : 0, commands.size()}, std::move(action), revision});
        return true;
    }

    void FrozenPrepareContext::Begin()
    {
        // Copy candidates after predecessors publish, not when the plan is bound.
        for (const auto &binding : bindings) {
            if (binding->writeObject) {
                for (auto &row : binding->rows) {
                    row.staged = *row.object;
                }
            }
        }
        scope->owner = std::this_thread::get_id();
        scope->active.store(true, std::memory_order_release);
    }

    std::optional<PrepareChunkRange> FrozenPrepareContext::Chunk() const
    {
        return chunk ? std::optional{chunk->range} : std::nullopt;
    }

    std::span<std::byte> FrozenPrepareContext::Scratch() const noexcept
    {
        if (!chunk || !scope->active.load(std::memory_order_acquire) || scope->owner != std::this_thread::get_id()) {
            return {};
        }
        return chunk->scratch;
    }

    void FrozenPrepareContext::End(bool success) noexcept
    {
        scope->active.store(false, std::memory_order_release);
        if (success) {
            Publish();
        }
    }

    void FrozenPrepareContext::Publish() noexcept
    {
        static_assert(std::is_nothrow_copy_assignable_v<RenderObjectData>);
        for (const auto &binding : bindings) {
            if (binding->writeObject) {
                for (auto &row : binding->rows) {
                    if (row.written) {
                        row.staged.sourceRevision   = row.revision;
                        row.staged.preparedSequence = frame->Sequence();
                        row.staged.valid            = true;
                        *row.object                 = row.staged;
                    }
                }
            }
        }
    }

} // namespace sky::aurora
