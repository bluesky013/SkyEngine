#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <framework/serialization/Any.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky {
    class ThreadPool;
    struct TypeNode;
} // namespace sky

namespace sky::editor {

    enum class AssetGeneratorCode {
        Success,
        InvalidArgument,
        DuplicateId,
        Unavailable,
        Unsupported,
        InvalidSettings,
        Pending,
        Cancelled,
        StaleInput,
        Failed,
        AlreadyConsumed,
        END
    };

    struct AssetGeneratorStatus {
        AssetGeneratorCode code = AssetGeneratorCode::Success;
        std::string        message;
        explicit           operator bool() const
        {
            return code == AssetGeneratorCode::Success;
        }
    };

    struct AssetGeneratorContext {
        Uuid source;
        // A new document identity is required on reopen; revisions include every settings/control edit, including undo.
        Uuid                     document;
        std::string              inputType;
        uint64_t                 sourceRevision = 0;
        std::vector<std::string> requiredCapabilities;
        bool                     operator==(const AssetGeneratorContext &) const = default;
    };

    // Each UUID names a documented consumer interface, never a concrete implementation.
    // The returned interface remains valid while this immutable owning payload is retained.
    class IAssetGeneratorPayload {
    public:
        virtual ~IAssetGeneratorPayload()                                 = default;
        virtual const void *QueryInterface(const Uuid &interfaceId) const = 0;
    };

    struct AssetGeneratorDescriptor {
        std::string              id;
        std::string              label;
        std::vector<std::string> inputTypes;
        std::vector<std::string> capabilities;
        const TypeNode          *settingsType = nullptr;
    };

    struct AssetGeneratorIdentity {
        std::string id;
        Uuid        owner;
        uint64_t    generation                                       = 0;
        bool        operator==(const AssetGeneratorIdentity &) const = default;
    };

    struct AssetGeneratorSnapshot {
        AssetGeneratorIdentity   identity;
        AssetGeneratorDescriptor descriptor;
    };

    struct AssetGenerationInput {
        AssetGeneratorContext                         context;
        Any                                           settings;
        std::shared_ptr<const IAssetGeneratorPayload> payload;
    };

    struct AssetGenerationOutput {
        AssetGeneratorStatus                          status;
        std::shared_ptr<const IAssetGeneratorPayload> payload;
    };

    struct AssetGenerationResult {
        AssetGeneratorIdentity                        provider;
        AssetGeneratorContext                         context;
        std::shared_ptr<const IAssetGeneratorPayload> payload;
    };

    enum class AssetGenerationState { Pending, Running, Succeeded, Failed, Cancelled, END };

    struct AssetGenerationProgress {
        AssetGenerationState state           = AssetGenerationState::Pending;
        float                fraction        = 0.0f;
        bool                 cancelRequested = false;
        std::string          stage;
        AssetGeneratorStatus status{AssetGeneratorCode::Pending, {}};
    };

    struct AssetGenerationRequestState;
    struct AssetGeneratorRegistration;

    class AssetGenerationControl {
    public:
        bool IsCancellationRequested() const;
        // Borrow only during synchronous Generate; the control owns the flag's storage.
        const std::atomic_bool &CancellationFlag() const;
        AssetGeneratorStatus    ReportProgress(float fraction, std::string stage);

    private:
        friend class AssetGeneratorRegistry;
        explicit AssetGenerationControl(std::shared_ptr<AssetGenerationRequestState> state);
        std::shared_ptr<AssetGenerationRequestState> state;
    };

    class IAssetGenerator {
    public:
        virtual ~IAssetGenerator()                        = default;
        virtual AssetGeneratorDescriptor Describe() const = 0;
        // Settings metadata is process-owned; copied settings must own their mutable data.
        virtual AssetGeneratorStatus CreateSettings(Any &out) const                           = 0;
        virtual AssetGeneratorStatus CheckSupport(const AssetGeneratorContext &context) const = 0;
        // Runs on the supplied pool and returns only after its own work drains.
        // No registry/UI calls or borrowed mutable input; cancellation is cooperative.
        virtual AssetGenerationOutput Generate(const AssetGenerationInput &input, AssetGenerationControl &control) const = 0;
    };

    class AssetGenerationRequest {
    public:
        void                    Cancel();
        AssetGenerationProgress Poll() const;
        // Owner/join path only; never wait on a worker of the producing pool.
        void                 Wait() const;
        AssetGeneratorStatus TakeResult(const AssetGeneratorContext &current, AssetGenerationResult &out);

    private:
        friend class AssetGeneratorRegistry;
        explicit AssetGenerationRequest(std::shared_ptr<AssetGenerationRequestState> state);
        std::shared_ptr<AssetGenerationRequestState> state;
    };

    // Registry operations and result application belong to the editor host thread.
    // Requests are thread-safe; the supplied pool must outlive all its requests.
    class AssetGeneratorRegistry : public Singleton<AssetGeneratorRegistry> {
        friend class Singleton<AssetGeneratorRegistry>;

    public:
        ~AssetGeneratorRegistry() override;
        AssetGeneratorStatus Register(const Uuid &owner, std::shared_ptr<IAssetGenerator> provider);
        void                 UnregisterOwner(const Uuid &owner);
        uint64_t             Revision() const
        {
            return revision;
        }
        AssetGeneratorSnapshot              Find(const std::string &id) const;
        std::vector<AssetGeneratorSnapshot> List(const AssetGeneratorContext &context) const;
        bool                                IsCurrent(const AssetGeneratorSnapshot &snapshot) const;
        AssetGeneratorStatus                CreateSettings(const AssetGeneratorSnapshot &snapshot, Any &out) const;
        AssetGeneratorStatus                CheckSupport(const AssetGeneratorSnapshot &snapshot, const AssetGeneratorContext &context) const;
        AssetGeneratorStatus                Start(const AssetGeneratorSnapshot            &snapshot,
                                                  const AssetGenerationInput              &input,
                                                  ThreadPool                              &pool,
                                                  std::shared_ptr<AssetGenerationRequest> &out);
        AssetGeneratorStatus                ValidateResult(const AssetGenerationResult &result, const AssetGeneratorContext &current) const;

    private:
        std::unordered_map<std::string, std::shared_ptr<AssetGeneratorRegistration>> generators;
        uint64_t                                                                     revision = 0;
    };

} // namespace sky::editor
