#include <editor/core/asset/AssetGeneratorRegistry.h>

#include <core/async/Completion.h>
#include <core/async/ThreadPool.h>
#include <framework/serialization/SerializationContext.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace sky::editor {

    struct AssetGeneratorRegistration {
        AssetGeneratorSnapshot                                  snapshot;
        std::shared_ptr<IAssetGenerator>                        provider;
        std::atomic_bool                                        active{true};
        std::vector<std::weak_ptr<AssetGenerationRequestState>> requests;
    };

    struct AssetGenerationRequestState {
        AssetGenerationInput                          input;
        std::shared_ptr<AssetGeneratorRegistration>   registration;
        CompletionSource                              completion;
        std::shared_future<void>                      task;
        std::atomic_bool                              cancelled{false};
        mutable std::mutex                            mutex;
        AssetGenerationProgress                       progress;
        std::shared_ptr<const IAssetGeneratorPayload> result;
        bool                                          consumed = false;
    };

    namespace {
        AssetGeneratorStatus Unavailable()
        {
            return {AssetGeneratorCode::Unavailable, "Generator registration is no longer available"};
        }

        bool ValidContext(const AssetGeneratorContext &context)
        {
            return context.source && context.document && !context.inputType.empty();
        }

        bool UniqueNames(const std::vector<std::string> &names)
        {
            std::unordered_set<std::string> unique;
            for (const auto &name : names) {
                if (name.empty() || !unique.insert(name).second) {
                    return false;
                }
            }
            return true;
        }

        bool ValidSettingsType(const TypeNode *node)
        {
            return node && node->info && node->info->staticInfo && node->info->copy && !node->info->staticInfo->isPointer &&
                   GetTypeNode(node->info) == node;
        }

        AssetGeneratorStatus ValidateSettings(const TypeNode *node, const Any &settings)
        {
            if (!node) {
                return settings ? AssetGeneratorStatus{AssetGeneratorCode::InvalidSettings, "Generator has no settings schema"}
                                : AssetGeneratorStatus{};
            }
            if (!ValidSettingsType(node) || !settings || !settings.Info()->copy || settings.Info()->staticInfo->isPointer ||
                GetTypeNode(settings) != node) {
                return {AssetGeneratorCode::InvalidSettings, "Settings must be an owning copyable value of the registered schema"};
            }
            return {};
        }

        bool Supports(const AssetGeneratorDescriptor &descriptor, const AssetGeneratorContext &context)
        {
            if (std::find(descriptor.inputTypes.begin(), descriptor.inputTypes.end(), context.inputType) == descriptor.inputTypes.end()) {
                return false;
            }
            return std::all_of(context.requiredCapabilities.begin(), context.requiredCapabilities.end(), [&](const auto &capability) {
                return std::find(descriptor.capabilities.begin(), descriptor.capabilities.end(), capability) != descriptor.capabilities.end();
            });
        }

        AssetGeneratorStatus CheckContext(const AssetGeneratorContext &expected, const AssetGeneratorContext &current)
        {
            if (!ValidContext(current) || current != expected) {
                return {AssetGeneratorCode::StaleInput, "Source, document, settings or controls changed during generation"};
            }
            return {};
        }

        void Finish(const std::shared_ptr<AssetGenerationRequestState> &state, AssetGenerationOutput output)
        {
            CompletionResult completion;
            {
                std::lock_guard lock(state->mutex);
                if (state->cancelled.load(std::memory_order_acquire) || !state->registration->active.load(std::memory_order_acquire)) {
                    output = {{AssetGeneratorCode::Cancelled, "Generation was cancelled"}, nullptr};
                } else if (output.status.code < AssetGeneratorCode::Success || output.status.code >= AssetGeneratorCode::END ||
                           output.status.code == AssetGeneratorCode::Pending || (output.status && !output.payload)) {
                    output = {{AssetGeneratorCode::Failed, "Generator returned an invalid terminal result"}, nullptr};
                }
                state->progress.status = std::move(output.status);
                if (state->progress.status) {
                    state->result            = std::move(output.payload);
                    state->progress.fraction = 1.0f;
                    state->progress.state    = AssetGenerationState::Succeeded;
                    completion.state         = CompletionState::SUCCEEDED;
                } else if (state->progress.status.code == AssetGeneratorCode::Cancelled) {
                    state->progress.state = AssetGenerationState::Cancelled;
                    completion.state      = CompletionState::CANCELLED;
                } else {
                    state->progress.state = AssetGenerationState::Failed;
                    completion.state      = CompletionState::FAILED;
                }
                completion.diagnostic = state->progress.status.message;
                state->completion.Publish(std::move(completion));
            }
        }
    } // namespace

    AssetGenerationControl::AssetGenerationControl(std::shared_ptr<AssetGenerationRequestState> state) : state(std::move(state))
    {
    }

    bool AssetGenerationControl::IsCancellationRequested() const
    {
        return state->cancelled.load(std::memory_order_acquire) || state->completion.IsCancelRequested();
    }

    const std::atomic_bool &AssetGenerationControl::CancellationFlag() const
    {
        return state->cancelled;
    }

    AssetGeneratorStatus AssetGenerationControl::ReportProgress(float fraction, std::string stage)
    {
        if (!std::isfinite(fraction) || fraction < 0.0f || fraction > 1.0f) {
            return {AssetGeneratorCode::InvalidArgument, "Progress must be finite and between zero and one"};
        }
        std::lock_guard lock(state->mutex);
        if (IsCancellationRequested()) {
            return {AssetGeneratorCode::Cancelled, "Generation was cancelled"};
        }
        if (state->progress.state != AssetGenerationState::Running) {
            return {AssetGeneratorCode::Unavailable, "Generation is not running"};
        }
        state->progress.fraction = fraction;
        state->progress.stage    = std::move(stage);
        return {};
    }

    AssetGenerationRequest::AssetGenerationRequest(std::shared_ptr<AssetGenerationRequestState> state) : state(std::move(state))
    {
    }

    void AssetGenerationRequest::Cancel()
    {
        {
            std::lock_guard lock(state->mutex);
            state->cancelled.store(true, std::memory_order_release);
        }
        state->completion.RequestCancel();
    }

    AssetGenerationProgress AssetGenerationRequest::Poll() const
    {
        std::lock_guard lock(state->mutex);
        auto            progress = state->progress;
        progress.cancelRequested = state->cancelled.load(std::memory_order_acquire);
        return progress;
    }

    void AssetGenerationRequest::Wait() const
    {
        state->task.wait();
    }

    AssetGeneratorStatus AssetGenerationRequest::TakeResult(const AssetGeneratorContext &current, AssetGenerationResult &out)
    {
        if (!state->registration->active.load(std::memory_order_acquire)) {
            return Unavailable();
        }
        if (const auto status = CheckContext(state->input.context, current); !status) {
            return status;
        }
        if (state->cancelled.load(std::memory_order_acquire)) {
            return {AssetGeneratorCode::Cancelled, "Generation was cancelled"};
        }
        if (state->completion.Poll().result.state == CompletionState::PENDING) {
            return {AssetGeneratorCode::Pending, "Generation has not completed"};
        }
        std::lock_guard lock(state->mutex);
        if (state->cancelled.load(std::memory_order_acquire)) {
            return {AssetGeneratorCode::Cancelled, "Generation was cancelled"};
        }
        if (!state->progress.status) {
            return state->progress.status;
        }
        if (state->consumed) {
            return {AssetGeneratorCode::AlreadyConsumed, "Generation result was already consumed"};
        }
        out             = {state->registration->snapshot.identity, state->input.context, std::move(state->result)};
        state->consumed = true;
        return {};
    }

    AssetGeneratorRegistry::~AssetGeneratorRegistry()
    {
        while (!generators.empty()) {
            UnregisterOwner(generators.begin()->second->snapshot.identity.owner);
        }
    }

    AssetGeneratorStatus AssetGeneratorRegistry::Register(const Uuid &owner, std::shared_ptr<IAssetGenerator> provider)
    {
        if (!owner || !provider) {
            return {AssetGeneratorCode::InvalidArgument, "Registration requires an owner and provider"};
        }
        auto descriptor = provider->Describe();
        if (descriptor.id.empty() || descriptor.label.empty() || descriptor.inputTypes.empty() || !UniqueNames(descriptor.inputTypes) ||
            !UniqueNames(descriptor.capabilities) || (descriptor.settingsType && !ValidSettingsType(descriptor.settingsType))) {
            return {AssetGeneratorCode::InvalidArgument, "Generator descriptor or reflected settings schema is invalid"};
        }
        if (generators.contains(descriptor.id)) {
            return {AssetGeneratorCode::DuplicateId, "Generator ID is already registered"};
        }
        const auto registration           = std::make_shared<AssetGeneratorRegistration>();
        registration->snapshot.identity   = {descriptor.id, owner, ++revision};
        registration->snapshot.descriptor = std::move(descriptor);
        registration->provider            = std::move(provider);
        generators.emplace(registration->snapshot.identity.id, registration);
        return {};
    }

    void AssetGeneratorRegistry::UnregisterOwner(const Uuid &owner)
    {
        std::vector<std::shared_ptr<AssetGeneratorRegistration>> removed;
        for (auto it = generators.begin(); it != generators.end();) {
            if (it->second->snapshot.identity.owner == owner) {
                it->second->active.store(false, std::memory_order_release);
                removed.emplace_back(it->second);
                it = generators.erase(it);
            } else {
                ++it;
            }
        }
        if (!removed.empty())
            ++revision;
        std::vector<std::shared_ptr<AssetGenerationRequestState>> requests;
        for (const auto &registration : removed) {
            for (const auto &weak : registration->requests) {
                if (const auto state = weak.lock()) {
                    {
                        std::lock_guard lock(state->mutex);
                        state->cancelled.store(true, std::memory_order_release);
                    }
                    state->completion.RequestCancel();
                    requests.emplace_back(state);
                }
            }
        }
        for (const auto &state : requests) {
            state->task.wait();
        }
        for (const auto &registration : removed) {
            registration->provider.reset();
        }
    }

    AssetGeneratorSnapshot AssetGeneratorRegistry::Find(const std::string &id) const
    {
        const auto found = generators.find(id);
        return found == generators.end() ? AssetGeneratorSnapshot{} : found->second->snapshot;
    }

    std::vector<AssetGeneratorSnapshot> AssetGeneratorRegistry::List(const AssetGeneratorContext &context) const
    {
        std::vector<AssetGeneratorSnapshot> snapshots;
        for (const auto &[id, registration] : generators) {
            snapshots.emplace_back(registration->snapshot);
        }
        std::erase_if(snapshots, [&](const auto &snapshot) { return !CheckSupport(snapshot, context); });
        std::erase_if(snapshots, [&](const auto &snapshot) { return !IsCurrent(snapshot); });
        std::sort(snapshots.begin(), snapshots.end(), [](const auto &a, const auto &b) { return a.identity.id < b.identity.id; });
        return snapshots;
    }

    bool AssetGeneratorRegistry::IsCurrent(const AssetGeneratorSnapshot &snapshot) const
    {
        const auto found = generators.find(snapshot.identity.id);
        return found != generators.end() && snapshot.identity.generation != 0 && found->second->snapshot.identity == snapshot.identity;
    }

    AssetGeneratorStatus AssetGeneratorRegistry::CreateSettings(const AssetGeneratorSnapshot &snapshot, Any &out) const
    {
        if (!IsCurrent(snapshot)) {
            return Unavailable();
        }
        const auto registration = generators.at(snapshot.identity.id);
        const auto provider     = registration->provider;
        Any        settings;
        const auto status = provider->CreateSettings(settings);
        if (!IsCurrent(snapshot)) {
            return Unavailable();
        }
        if (!status) {
            return status;
        }
        if (const auto validation = ValidateSettings(registration->snapshot.descriptor.settingsType, settings); !validation) {
            return validation;
        }
        out = std::move(settings);
        return {};
    }

    AssetGeneratorStatus AssetGeneratorRegistry::CheckSupport(const AssetGeneratorSnapshot &snapshot, const AssetGeneratorContext &context) const
    {
        if (!IsCurrent(snapshot)) {
            return Unavailable();
        }
        if (!ValidContext(context) || !UniqueNames(context.requiredCapabilities)) {
            return {AssetGeneratorCode::InvalidArgument, "Generation requires a source, document and input type"};
        }
        const auto registration = generators.at(snapshot.identity.id);
        const auto provider     = registration->provider;
        if (!Supports(registration->snapshot.descriptor, context)) {
            return {AssetGeneratorCode::Unsupported, "Generator does not support the requested input or capabilities"};
        }
        const auto status = provider->CheckSupport(context);
        return IsCurrent(snapshot) ? status : Unavailable();
    }

    AssetGeneratorStatus AssetGeneratorRegistry::Start(const AssetGeneratorSnapshot            &snapshot,
                                                       const AssetGenerationInput              &input,
                                                       ThreadPool                              &pool,
                                                       std::shared_ptr<AssetGenerationRequest> &out)
    {
        if (const auto status = CheckSupport(snapshot, input.context); !status) {
            return status;
        }
        const auto registration = generators.at(snapshot.identity.id);
        if (const auto status = ValidateSettings(registration->snapshot.descriptor.settingsType, input.settings); !status) {
            return status;
        }
        if (!input.payload || !pool.GetThreadCount()) {
            return {AssetGeneratorCode::InvalidArgument, "Generation requires owned input and an active worker pool"};
        }
        const auto state    = std::make_shared<AssetGenerationRequestState>();
        state->input        = input;
        state->registration = registration;
        if (!IsCurrent(snapshot)) {
            return Unavailable();
        }
        const auto request = std::shared_ptr<AssetGenerationRequest>(new AssetGenerationRequest(state));
        const auto task    = pool.CreateTask([state](ThreadContext &) {
            AssetGenerationOutput output;
            if (state->cancelled.load(std::memory_order_acquire)) {
                output.status = {AssetGeneratorCode::Cancelled, "Generation was cancelled before starting"};
            } else {
                {
                    std::lock_guard lock(state->mutex);
                    state->progress.state = AssetGenerationState::Running;
                }
                AssetGenerationControl control(state);
                output = state->registration->provider->Generate(state->input, control);
            }
            Finish(state, std::move(output));
        });
        state->task        = task->GetFuture().share();
        std::erase_if(registration->requests, [](const auto &weak) { return weak.expired(); });
        registration->requests.emplace_back(state);
        out = request;
        pool.Submit(task);
        return {};
    }

    AssetGeneratorStatus AssetGeneratorRegistry::ValidateResult(const AssetGenerationResult &result, const AssetGeneratorContext &current) const
    {
        if (!IsCurrent({result.provider, {}})) {
            return Unavailable();
        }
        if (const auto status = CheckContext(result.context, current); !status) {
            return status;
        }
        return result.payload ? AssetGeneratorStatus{}
                              : AssetGeneratorStatus{AssetGeneratorCode::InvalidArgument, "Generation result has no payload"};
    }

} // namespace sky::editor
