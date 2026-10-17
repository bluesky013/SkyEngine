#include "prepare/ScenePrepareScheduler.h"

#include <algorithm>
#include <core/async/TaskDispatcher.h>
#include <map>

namespace sky::aurora {

    struct PrepareJobGroup::State {
        struct SystemWork {
            std::vector<FrozenPrepareContext> contexts;
            std::vector<PrepareNodeResult>    outcomes;
            TaskDispatcher::NodeIndex         entry       = TaskDispatcher::kInvalidNode;
            TaskDispatcher::NodeIndex         exit        = TaskDispatcher::kInvalidNode;
            bool                              partitioned = false;
        };
        bool Execute(size_t index, size_t chunk)
        {
            auto &work    = systems[index];
            auto  outcome = completion->IsCancelRequested()
                                ? PrepareNodeResult{result.nodes[index].system, PrepareNodeState::SKIPPED, "Prepare cancelled before kernel entry"}
                                : FrozenPrepareExecutor::Invoke(work.contexts[chunk], kernel, !work.partitioned);
            if (work.partitioned) {
                work.outcomes[chunk] = std::move(outcome);
                // Kernel failures are resolved at the join so every sibling drains before publication or discard.
                return true;
            }
            result.nodes[index] = std::move(outcome);
            return result.nodes[index].state == PrepareNodeState::SUCCEEDED;
        }
        bool Join(size_t index)
        {
            auto             &work = systems[index];
            PrepareNodeResult outcome{result.nodes[index].system, PrepareNodeState::SUCCEEDED};
            for (auto &chunk : work.outcomes) {
                if (chunk.state == PrepareNodeState::FAILED ||
                    (chunk.state != PrepareNodeState::SUCCEEDED && outcome.state == PrepareNodeState::SUCCEEDED)) {
                    outcome.state      = chunk.state;
                    outcome.diagnostic = std::move(chunk.diagnostic);
                    if (chunk.state == PrepareNodeState::FAILED)
                        break;
                }
            }
            if (outcome.state == PrepareNodeState::SUCCEEDED) {
                for (auto &context : work.contexts)
                    FrozenPrepareExecutor::Publish(context);
            }
            result.nodes[index] = std::move(outcome);
            return result.nodes[index].state == PrepareNodeState::SUCCEEDED;
        }
        void Finish(DispatchState dispatch)
        {
            const bool failed = !result.error.empty() || std::any_of(result.nodes.begin(), result.nodes.end(),
                                                                     [](const auto &node) { return node.state == PrepareNodeState::FAILED; });
            result.cancelled  = !failed && completion->IsCancelRequested();
            if (result.cancelled)
                result.error = "CPU prepare cancelled";
            else if (dispatch != DispatchState::SUCCEEDED && result.Succeeded())
                result.error = "Prepare dispatcher failed";
            if (products && result.Succeeded()) {
                for (const auto &system : systems) {
                    for (const auto &context : system.contexts)
                        FrozenPrepareExecutor::MergeOutputs(context, *products);
                }
                result.products = std::move(products);
            }
            products.reset();
            systems.clear();
            kernel = {};
            dispatcher.Clear();
            const auto terminal   = result.cancelled     ? CompletionState::CANCELLED
                                    : result.Succeeded() ? CompletionState::SUCCEEDED
                                                         : CompletionState::FAILED;
            auto       diagnostic = result.error;
            if (terminal == CompletionState::FAILED && diagnostic.empty()) {
                const auto first =
                    std::find_if(result.nodes.begin(), result.nodes.end(), [](const auto &node) { return node.state == PrepareNodeState::FAILED; });
                diagnostic = first != result.nodes.end() ? first->diagnostic : "Prepare dependency did not succeed";
            }
            completion->Publish({terminal, std::move(diagnostic)});
        }
        TaskDispatcher                    dispatcher;
        std::shared_ptr<CompletionSource> completion = std::make_shared<CompletionSource>();
        std::vector<SystemWork>           systems;
        std::shared_ptr<PrepareProducts>  products;
        PrepareKernel                     kernel;
        PrepareResult                     result;
        bool                              serialFallback = false;
    };

    PrepareJobGroup::PrepareJobGroup(std::shared_ptr<State> state) : state(std::move(state))
    {
    }
    PrepareJobGroup::~PrepareJobGroup()
    {
        Wait();
    }

    const PrepareResult &PrepareJobGroup::Wait()
    {
        state->completion->Wait();
        return state->result;
    }

    CompletionState PrepareJobGroup::Poll()
    {
        return state->completion->Poll().result.state;
    }

    CompletionToken PrepareJobGroup::GetCompletion() const
    {
        return state->completion;
    }
    void PrepareJobGroup::RequestCancel()
    {
        state->completion->RequestCancel();
    }

    bool PrepareJobGroup::UsedSerialFallback() const
    {
        return state->serialFallback;
    }

    struct ScenePrepareScheduler::State {
        explicit State(const ScenePrepareInit &info) : info(info), pool(info.workers)
        {
        }
        ScenePrepareInit                 info;
        ThreadPool                       pool;
        std::shared_ptr<PrepareJobGroup> active;
    };

    ScenePrepareScheduler::ScenePrepareScheduler(std::unique_ptr<State> state) : state(std::move(state))
    {
    }
    PrepareSchedulerCreateResult ScenePrepareScheduler::Create(const ScenePrepareInit &info)
    {
        if (info.workers == 0)
            return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Prepare workers must be nonzero"}};
        return {std::unique_ptr<ScenePrepareScheduler>(new ScenePrepareScheduler(std::make_unique<State>(info))), {}};
    }

    ScenePrepareScheduler::~ScenePrepareScheduler()
    {
        if (state->active)
            state->active->Wait();
    }

    PrepareSubmitResult ScenePrepareScheduler::Submit(const FrozenCpuFrame            &frame,
                                                      std::vector<SystemDesc>          systems,
                                                      PrepareKernel                    kernel,
                                                      std::vector<ElementWisePrepare>  partitions,
                                                      std::vector<PrepareOutputConfig> outputs)
    {
        if (state->active && state->active->Poll() == CompletionState::PENDING) {
            return {{}, RendererError{RendererErrorCode::NOT_READY, "Prepare scheduler already has an active group"}};
        }
        auto compiled = CompileDependencyPlan(systems);
        if (const auto *error = std::get_if<DependencyDiagnostic>(&compiled)) {
            return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, error->message}};
        }
        const auto &plan = std::get<DependencyPlan>(compiled);
        if (!plan.systems.empty() && !kernel) {
            return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Missing CPU prepare kernel"}};
        }
        std::map<SystemId, size_t> indices;
        for (size_t i = 0; i < plan.systems.size(); ++i)
            indices.emplace(plan.systems[i].id, i);
        std::vector<const ElementWisePrepare *> options(plan.systems.size(), nullptr);
        for (const auto &partition : partitions) {
            const auto found = indices.find(partition.system);
            if (found == indices.end() || options[found->second] || partition.rowsPerChunk == 0) {
                return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid or duplicate element-wise declaration"}};
            }
            options[found->second] = &partition;
        }
        std::vector<const PrepareOutputConfig *> outputOptions(plan.systems.size(), nullptr);
        for (const auto &output : outputs) {
            const auto found = indices.find(output.system);
            if (found == indices.end() || outputOptions[found->second]) {
                return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid or duplicate output declaration"}};
            }
            outputOptions[found->second] = &output;
        }
        std::vector<FrozenPrepareContext> contexts;
        if (auto error = FrozenPrepareExecutor::Bind(frame, plan, state->info.maxBoundRows, contexts)) {
            return {{}, std::move(error)};
        }
        size_t              remainingNodes    = state->info.maxTaskNodes;
        size_t              remainingScratch  = state->info.maxScratchBytes;
        size_t              remainingRecords  = state->info.maxOutputRecords;
        size_t              remainingBytes    = state->info.maxOutputBytes;
        size_t              remainingDeferred = state->info.maxDeferredCommands;
        std::vector<size_t> rowCounts(plan.systems.size(), 0);
        std::vector<size_t> chunkCounts(plan.systems.size(), 1);
        for (size_t i = 0; i < plan.systems.size(); ++i) {
            if (const auto *option = options[i]) {
                auto rows = FrozenPrepareExecutor::PartitionRows(contexts[i], option->scene);
                if (!rows) {
                    return {{},
                            RendererError{RendererErrorCode::INVALID_ARGUMENT, "Element-wise work requires one scene and no shared resource writes"}};
                }
                rowCounts[i]   = *rows;
                chunkCounts[i] = *rows == 0 ? 1 : 1 + (*rows - 1) / option->rowsPerChunk;
                if (option->scratchBytes && chunkCounts[i] > remainingScratch / option->scratchBytes) {
                    return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Prepare scratch budget exceeded"}};
                }
                remainingScratch -= chunkCounts[i] * option->scratchBytes;
            }
            if (chunkCounts[i] > remainingNodes || (options[i] && chunkCounts[i] == remainingNodes)) {
                return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Prepare task node budget exceeded"}};
            }
            remainingNodes -= chunkCounts[i] + (options[i] ? 1 : 0);
            if (const auto *output = outputOptions[i]) {
                const auto reserve = [&](size_t perTask, size_t &remaining) {
                    if (perTask && chunkCounts[i] > remaining / perTask)
                        return false;
                    remaining -= perTask * chunkCounts[i];
                    return true;
                };
                if (!reserve(output->maxRecordsPerTask, remainingRecords) || !reserve(output->maxBytesPerTask, remainingBytes) ||
                    !reserve(output->maxDeferredPerTask, remainingDeferred)) {
                    return {{}, RendererError{RendererErrorCode::INVALID_ARGUMENT, "Prepare output or deferred budget exceeded"}};
                }
            }
        }
        auto data = std::make_shared<PrepareJobGroup::State>();
        data->systems.resize(plan.systems.size());
        data->result.nodes.reserve(plan.systems.size());
        if (!outputs.empty()) {
            data->products = std::make_shared<PrepareProducts>();
            data->products->records.reserve(state->info.maxOutputRecords - remainingRecords);
            data->products->bytes.reserve(state->info.maxOutputBytes - remainingBytes);
            data->products->deferred.commands.reserve(state->info.maxDeferredCommands - remainingDeferred);
            data->products->deferred.sourceSequence = frame.Sequence();
        }
        for (size_t i = 0; i < plan.systems.size(); ++i) {
            auto &work = data->systems[i];
            data->result.nodes.push_back({plan.systems[i].id, PrepareNodeState::SKIPPED, "Prepare dependency did not succeed"});
            if (const auto *option = options[i]) {
                work.partitioned = true;
                work.contexts.reserve(chunkCounts[i]);
                work.outcomes.resize(chunkCounts[i]);
                for (size_t j = 0, first = 0; j < chunkCounts[i]; ++j) {
                    const auto count = std::min(option->rowsPerChunk, rowCounts[i] - first);
                    work.contexts.push_back(FrozenPrepareExecutor::MakeChunk(contexts[i], {j, chunkCounts[i], first, count}, option->scratchBytes));
                    first += count;
                }
            } else {
                work.contexts.push_back(std::move(contexts[i]));
            }
            if (const auto *output = outputOptions[i]) {
                for (auto &context : work.contexts)
                    FrozenPrepareExecutor::ConfigureOutputs(context, *output);
            }
        }
        contexts.clear();
        data->kernel = std::move(kernel);
        const auto budget =
            std::min<uint64_t>(state->info.maxParallelNodes, uint64_t{state->pool.GetThreadCount()} * ThreadPool::GetLocalQueueCapacity());
        auto  group = std::shared_ptr<PrepareJobGroup>(new PrepareJobGroup(std::move(data)));
        auto &batch = *group->state;
        if (state->info.maxTaskNodes - remainingNodes > budget) {
            batch.serialFallback = true;
            batch.dispatcher.CreateCheckedTask([ptr = &batch, plan, indices = std::move(indices)](ThreadContext &) {
                ptr->result = ExecuteSerialPrepareChecked(plan, [&](SystemId id) {
                    const auto index = indices.at(id);
                    auto      &work  = ptr->systems[index];
                    for (size_t j = 0; j < work.contexts.size(); ++j)
                        ptr->Execute(index, j);
                    if (work.partitioned)
                        ptr->Join(index);
                    return std::move(ptr->result.nodes[index]);
                });
                return ptr->result.Succeeded();
            });
        } else {
            for (size_t i = 0; i < plan.systems.size(); ++i) {
                auto &work = batch.systems[i];
                for (size_t j = 0; j < work.contexts.size(); ++j) {
                    const auto node = batch.dispatcher.CreateCheckedTask([ptr = &batch, i, j](ThreadContext &) { return ptr->Execute(i, j); });
                    if (j == 0)
                        work.entry = node;
                    work.exit = node;
                }
                if (work.partitioned) {
                    work.exit = batch.dispatcher.CreateCheckedTask([ptr = &batch, i](ThreadContext &) { return ptr->Join(i); });
                    for (size_t j = 0; j < work.contexts.size(); ++j)
                        batch.dispatcher.DependsOn(work.exit, work.entry + static_cast<uint32_t>(j));
                }
            }
            for (const auto &edge : plan.edges) {
                const auto &work = batch.systems[edge.to];
                for (size_t j = 0; j < work.contexts.size(); ++j) {
                    batch.dispatcher.DependsOn(work.entry + static_cast<uint32_t>(j), batch.systems[edge.from].exit);
                }
            }
        }
        auto dispatch = batch.dispatcher.Submit(state->pool, [pin = group->state](DispatchState result) { pin->Finish(result); });
        if (!dispatch.Accepted()) {
            batch.result.error = "Prepare dispatcher rejected the batch";
            batch.Finish(DispatchState::FAILED);
            return {{}, RendererError{RendererErrorCode::INTERNAL_ERROR, batch.result.error}};
        }
        state->active = group;
        return {std::move(group), {}};
    }

} // namespace sky::aurora
