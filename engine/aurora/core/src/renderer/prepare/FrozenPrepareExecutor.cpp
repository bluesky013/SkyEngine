#include "prepare/FrozenPrepareExecutor.h"

#include <map>
#include <set>

namespace sky::aurora {

    SerialPrepareResult FrozenPrepareExecutor::Run(const FrozenCpuFrame                                    &frame,
                                                   const DependencyPlan                                    &plan,
                                                   const std::function<void(const FrozenPrepareContext &)> &kernel)
    {
        if (!plan.systems.empty() && !kernel)
            return {{}, "Missing frozen prepare kernel"};
        std::vector<FrozenPrepareContext> contexts;
        if (auto error = Bind(frame, plan, std::numeric_limits<size_t>::max(), contexts)) {
            return {{}, error->message.empty() ? "Prepare binding failed" : std::move(error->message)};
        }
        std::map<SystemId, size_t> indices;
        for (size_t i = 0; i < plan.systems.size(); ++i)
            indices.emplace(plan.systems[i].id, i);
        return ExecuteSerialPrepareChecked(plan, [&](SystemId id) { return Invoke(contexts[indices.at(id)], kernel); });
    }

    std::optional<RendererError>
    FrozenPrepareExecutor::Bind(const FrozenCpuFrame &frame, const DependencyPlan &plan, size_t maxRows, std::vector<FrozenPrepareContext> &contexts)
    {
        if (frame.prepareStarted.exchange(true)) {
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "A frozen frame accepts only one prepare execution"};
        }
        contexts.reserve(plan.systems.size());
        for (const auto &system : plan.systems) {
            std::set<AccessOwner> owners;
            for (const auto &access : system.accesses) {
                const auto owner = access.key.owner;
                if (access.key.kind != AccessKind::COMPONENT || !owners.insert(owner).second || owner.instance == 0 ||
                    owner.instance > std::numeric_limits<uint32_t>::max() || owner.generation == 0 ||
                    owner.generation > std::numeric_limits<uint32_t>::max())
                    continue;
                const auto *scene = frame.FindScene({static_cast<uint32_t>(owner.instance - 1), static_cast<uint32_t>(owner.generation)});
                if (scene) {
                    if (scene->entities.size() > maxRows) {
                        contexts.clear();
                        return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Prepare binding row budget exceeded"};
                    }
                    maxRows -= scene->entities.size();
                }
            }
            auto bound = frame.Bind(system);
            if (!bound.context) {
                contexts.clear();
                return std::move(bound.error);
            }
            contexts.push_back(std::move(*bound.context));
        }
        return {};
    }

    std::optional<size_t> FrozenPrepareExecutor::PartitionRows(const FrozenPrepareContext &context, SceneId scene)
    {
        if (context.bindings.size() != 1 || context.bindings.front()->scene != scene)
            return {};
        for (const auto &access : context.accesses) {
            if (access.key.kind == AccessKind::RESOURCE && access.mode == AccessMode::WRITE)
                return {};
        }
        return context.bindings.front()->rows.size();
    }

    FrozenPrepareContext FrozenPrepareExecutor::MakeChunk(const FrozenPrepareContext &context, PrepareChunkRange range, size_t scratchBytes)
    {
        SystemDesc desc;
        desc.id       = context.system;
        desc.accesses = context.accesses;
        FrozenPrepareContext result(context.frame, desc);
        const auto          &source  = *context.bindings.front();
        auto                 binding = std::make_shared<FrozenSceneBinding>();
        binding->scene               = source.scene;
        binding->writeObject         = source.writeObject;
        binding->rows.assign(source.rows.begin() + range.first, source.rows.begin() + range.first + range.count);
        result.bindings.push_back(std::move(binding));
        result.chunk        = std::make_shared<PrepareChunkStorage>();
        result.chunk->range = range;
        result.chunk->scratch.resize(scratchBytes);
        return result;
    }

    void FrozenPrepareExecutor::Publish(FrozenPrepareContext &context) noexcept
    {
        context.Publish();
    }

    void FrozenPrepareExecutor::ConfigureOutputs(FrozenPrepareContext &context, const PrepareOutputConfig &config)
    {
        context.products         = std::make_shared<PrepareTaskProducts>();
        context.products->limits = config;
        auto &data               = context.products->data;
        data.records.reserve(config.maxRecordsPerTask);
        data.bytes.reserve(config.maxBytesPerTask);
        data.deferred.commands.reserve(config.maxDeferredPerTask);
    }

    void FrozenPrepareExecutor::MergeOutputs(const FrozenPrepareContext &context, PrepareProducts &result)
    {
        if (!context.products)
            return;
        const auto &data   = context.products->data;
        const auto  offset = result.bytes.size();
        result.bytes.insert(result.bytes.end(), data.bytes.begin(), data.bytes.end());
        for (auto record : data.records) {
            record.offset += offset;
            result.records.push_back(record);
        }
        result.deferred.commands.insert(result.deferred.commands.end(), data.deferred.commands.begin(), data.deferred.commands.end());
    }

    PrepareNodeResult
    FrozenPrepareExecutor::Invoke(FrozenPrepareContext &context, const std::function<void(const FrozenPrepareContext &)> &kernel, bool publish)
    {
        PrepareNodeResult result{context.Id(), PrepareNodeState::FAILED};
        context.Begin();
        kernel(context);
        if (!context.scope->failed)
            result.state = PrepareNodeState::SUCCEEDED;
        result.diagnostic = std::move(context.scope->diagnostic);
        if (result.state == PrepareNodeState::FAILED && result.diagnostic.empty())
            result.diagnostic = "Invalid prepare access";
        context.End(publish && result.state == PrepareNodeState::SUCCEEDED);
        return result;
    }

} // namespace sky::aurora
