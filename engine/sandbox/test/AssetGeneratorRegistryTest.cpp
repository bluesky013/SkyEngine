#include <editor/core/asset/AssetGeneratorRegistry.h>

#include "TestTypes.h"

#include <core/async/ThreadPool.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>

namespace sky::editor::test {
    namespace {
        constexpr Uuid NUMBER_INTERFACE = Uuid::CreateFromString("f40bcaa0-0dd1-4f5f-8f81-373267c78636");

        class INumber {
        public:
            virtual ~INumber()          = default;
            virtual float Value() const = 0;
        };

        class NumberPayload final : public IAssetGeneratorPayload, public INumber {
        public:
            explicit NumberPayload(float value) : value(value)
            {
            }
            const void *QueryInterface(const Uuid &id) const override
            {
                return id == NUMBER_INTERFACE ? static_cast<const INumber *>(this) : nullptr;
            }
            float Value() const override
            {
                return value;
            }

        private:
            float value;
        };

        class TestGenerator final : public IAssetGenerator {
        public:
            explicit TestGenerator(std::string id) : id(std::move(id))
            {
            }
            AssetGeneratorDescriptor Describe() const override
            {
                return {id, id, {"TestMesh"}, {"binding"}, GetTypeNode(TypeInfo<TestObject>::RegisteredId())};
            }
            AssetGeneratorStatus CreateSettings(Any &out) const override
            {
                out = Any(TestObject{2.0f, {3.0f, 4.0f}});
                return {};
            }
            AssetGeneratorStatus CheckSupport(const AssetGeneratorContext &) const override
            {
                return support ? support() : AssetGeneratorStatus{};
            }
            AssetGenerationOutput Generate(const AssetGenerationInput &input, AssetGenerationControl &control) const override
            {
                return generate ? generate(input, control) : AssetGenerationOutput{{}, input.payload};
            }

            std::string                                                                                  id;
            std::function<AssetGeneratorStatus()>                                                        support;
            std::function<AssetGenerationOutput(const AssetGenerationInput &, AssetGenerationControl &)> generate;
        };

        struct GateState {
            std::mutex              mutex;
            std::condition_variable changed;
            bool                    entered  = false;
            bool                    released = false;

            void Run(AssetGenerationControl &control)
            {
                std::unique_lock lock(mutex);
                entered = true;
                changed.notify_all();
                while (!released && !control.IsCancellationRequested()) {
                    changed.wait_for(lock, std::chrono::milliseconds(5));
                }
            }
        };

        class Gate {
        public:
            ~Gate()
            {
                Release();
            }
            bool WaitEntered()
            {
                std::unique_lock lock(state->mutex);
                return state->changed.wait_for(lock, std::chrono::seconds(5), [&] { return state->entered; });
            }
            void Release()
            {
                std::lock_guard lock(state->mutex);
                state->released = true;
                state->changed.notify_all();
            }
            std::shared_ptr<GateState> state = std::make_shared<GateState>();
        };

        class AssetGeneratorRegistryTest : public testing::Test {
        protected:
            void TearDown() override
            {
                registry->UnregisterOwner(owner);
                registry->UnregisterOwner(otherOwner);
            }
            AssetGeneratorContext Context() const
            {
                return {source, document, "TestMesh", 4, {"binding"}};
            }
            AssetGenerationInput Input() const
            {
                return {Context(), Any(TestObject{2.0f, {3.0f, 4.0f}}), std::make_shared<NumberPayload>(7.0f)};
            }

            AssetGeneratorRegistry *registry   = AssetGeneratorRegistry::Get();
            Uuid                    owner      = Uuid::Create();
            Uuid                    otherOwner = Uuid::Create();
            Uuid                    source     = Uuid::Create();
            Uuid                    document   = Uuid::Create();
        };

        TEST_F(AssetGeneratorRegistryTest, ProvidersCoexistAndDuplicateIdPreservesRegistration)
        {
            const auto first  = std::make_shared<TestGenerator>("test.generator.first");
            const auto second = std::make_shared<TestGenerator>("test.generator.second");
            ASSERT_TRUE(registry->Register(owner, first));
            ASSERT_TRUE(registry->Register(otherOwner, second));
            const auto snapshot = registry->Find(first->id);
            EXPECT_EQ(registry->Register(otherOwner, std::make_shared<TestGenerator>(first->id)).code, AssetGeneratorCode::DuplicateId);
            EXPECT_TRUE(registry->IsCurrent(snapshot));
            const auto listed = registry->List(Context());
            ASSERT_EQ(listed.size(), 2u);
            EXPECT_EQ(listed[0].identity.id, first->id);
            EXPECT_EQ(listed[1].identity.id, second->id);
            auto unsupported = Context();
            unsupported.requiredCapabilities.emplace_back("not-supported");
            EXPECT_TRUE(registry->List(unsupported).empty());
            registry->UnregisterOwner(owner);
            EXPECT_FALSE(registry->IsCurrent(snapshot));
            EXPECT_EQ(registry->List(Context()).size(), 1u);
        }

        TEST_F(AssetGeneratorRegistryTest, WithdrawnSnapshotCannotCreateSettingsOrStartReplacement)
        {
            ThreadPool pool(1);
            const auto provider = std::make_shared<TestGenerator>("test.generator.replaced");
            ASSERT_TRUE(registry->Register(owner, provider));
            const auto old = registry->Find(provider->id);
            registry->UnregisterOwner(owner);
            ASSERT_TRUE(registry->Register(owner, provider));
            EXPECT_NE(registry->Find(provider->id).identity.generation, old.identity.generation);
            Any settings(TestObject{19.0f, {}});
            EXPECT_EQ(registry->CreateSettings(old, settings).code, AssetGeneratorCode::Unavailable);
            EXPECT_EQ(settings.GetAsConst<TestObject>()->value, 19.0f);
            std::shared_ptr<AssetGenerationRequest> request;
            EXPECT_EQ(registry->Start(old, Input(), pool, request).code, AssetGeneratorCode::Unavailable);
            EXPECT_FALSE(request);
        }

        TEST_F(AssetGeneratorRegistryTest, SettingsSnapshotOwnsNestedDataAndResultUsesInterfaceIdentity)
        {
            ThreadPool pool(1);
            Gate       gate;
            const auto provider = std::make_shared<TestGenerator>("test.generator.snapshot");
            provider->generate  = [state = gate.state](const auto &input, auto &control) {
                state->Run(control);
                const auto *settings = input.settings.template GetAsConst<TestObject>();
                return AssetGenerationOutput{{}, std::make_shared<NumberPayload>(settings->value + settings->items.front())};
            };
            ASSERT_TRUE(registry->Register(owner, provider));
            auto                                    input = Input();
            std::shared_ptr<AssetGenerationRequest> request;
            ASSERT_TRUE(registry->Start(registry->Find(provider->id), input, pool, request));
            ASSERT_TRUE(gate.WaitEntered());
            input.settings.GetAs<TestObject>()->value         = 100.0f;
            input.settings.GetAs<TestObject>()->items.front() = 200.0f;
            input.payload.reset();
            gate.Release();
            request->Wait();
            AssetGenerationResult result;
            ASSERT_TRUE(request->TakeResult(Context(), result));
            EXPECT_EQ(result.provider.owner, owner);
            EXPECT_TRUE(registry->ValidateResult(result, Context()));
            const auto *number = static_cast<const INumber *>(result.payload->QueryInterface(NUMBER_INTERFACE));
            ASSERT_NE(number, nullptr);
            EXPECT_EQ(number->Value(), 5.0f);
            EXPECT_EQ(result.payload->QueryInterface(Uuid::Create()), nullptr);
            EXPECT_EQ(request->TakeResult(Context(), result).code, AssetGeneratorCode::AlreadyConsumed);
        }

        TEST_F(AssetGeneratorRegistryTest, StaleSourceDocumentAndRevisionPreservePreviousOutput)
        {
            ThreadPool pool(1);
            const auto provider = std::make_shared<TestGenerator>("test.generator.revision");
            ASSERT_TRUE(registry->Register(owner, provider));
            std::shared_ptr<AssetGenerationRequest> request;
            ASSERT_TRUE(registry->Start(registry->Find(provider->id), Input(), pool, request));
            request->Wait();
            const auto            previous = std::make_shared<NumberPayload>(11.0f);
            AssetGenerationResult result{{}, {}, previous};
            auto                  changed = Context();
            ++changed.sourceRevision;
            EXPECT_EQ(request->TakeResult(changed, result).code, AssetGeneratorCode::StaleInput);
            EXPECT_EQ(result.payload, previous);
            changed        = Context();
            changed.source = Uuid::Create();
            EXPECT_EQ(request->TakeResult(changed, result).code, AssetGeneratorCode::StaleInput);
            changed          = Context();
            changed.document = Uuid::Create();
            EXPECT_EQ(request->TakeResult(changed, result).code, AssetGeneratorCode::StaleInput);
            ASSERT_TRUE(request->TakeResult(Context(), result));
            EXPECT_EQ(registry->ValidateResult(result, changed).code, AssetGeneratorCode::StaleInput);
            registry->UnregisterOwner(owner);
            ASSERT_TRUE(registry->Register(owner, provider));
            EXPECT_EQ(registry->ValidateResult(result, Context()).code, AssetGeneratorCode::Unavailable);
        }

        TEST_F(AssetGeneratorRegistryTest, OwnerWithdrawalCancelsAndDrainsDroppedRequest)
        {
            ThreadPool pool(1);
            Gate       gate;
            const auto exited   = std::make_shared<std::atomic_bool>(false);
            const auto provider = std::make_shared<TestGenerator>("test.generator.drain");
            provider->generate  = [state = gate.state, exited](const auto &input, auto &control) {
                state->Run(control);
                exited->store(true, std::memory_order_release);
                return AssetGenerationOutput{{}, input.payload};
            };
            ASSERT_TRUE(registry->Register(owner, provider));
            std::shared_ptr<AssetGenerationRequest> request;
            ASSERT_TRUE(registry->Start(registry->Find(provider->id), Input(), pool, request));
            ASSERT_TRUE(gate.WaitEntered());
            request.reset();
            registry->UnregisterOwner(owner);
            EXPECT_TRUE(exited->load(std::memory_order_acquire));
        }

        TEST_F(AssetGeneratorRegistryTest, CancelledWorkCannotPublishSuccessAndReportsProgress)
        {
            ThreadPool pool(1);
            Gate       gate;
            const auto provider = std::make_shared<TestGenerator>("test.generator.cancel");
            provider->generate  = [state = gate.state](const auto &input, auto &control) {
                EXPECT_TRUE(control.ReportProgress(0.25f, "Preparing"));
                EXPECT_EQ(control.ReportProgress(std::numeric_limits<float>::quiet_NaN(), "Invalid").code, AssetGeneratorCode::InvalidArgument);
                state->Run(control);
                return AssetGenerationOutput{{}, input.payload};
            };
            ASSERT_TRUE(registry->Register(owner, provider));
            std::shared_ptr<AssetGenerationRequest> request;
            ASSERT_TRUE(registry->Start(registry->Find(provider->id), Input(), pool, request));
            ASSERT_TRUE(gate.WaitEntered());
            EXPECT_EQ(request->Poll().state, AssetGenerationState::Running);
            EXPECT_EQ(request->Poll().fraction, 0.25f);
            request->Cancel();
            request->Wait();
            EXPECT_EQ(request->Poll().state, AssetGenerationState::Cancelled);
            AssetGenerationResult result;
            EXPECT_EQ(request->TakeResult(Context(), result).code, AssetGeneratorCode::Cancelled);
            EXPECT_FALSE(result.payload);
        }

        TEST_F(AssetGeneratorRegistryTest, InvalidSettingsAndProviderFailurePreserveOutputs)
        {
            ThreadPool pool(1);
            const auto provider = std::make_shared<TestGenerator>("test.generator.failed");
            provider->generate  = [](const auto &, auto &) {
                return AssetGenerationOutput{{AssetGeneratorCode::Failed, "Invalid geometry"}, nullptr};
            };
            ASSERT_TRUE(registry->Register(owner, provider));
            const auto snapshot = registry->Find(provider->id);
            auto       input    = Input();
            input.settings      = Any(TestConfig{});
            std::shared_ptr<AssetGenerationRequest> request;
            EXPECT_EQ(registry->Start(snapshot, input, pool, request).code, AssetGeneratorCode::InvalidSettings);
            EXPECT_FALSE(request);
            ASSERT_TRUE(registry->Start(snapshot, Input(), pool, request));
            request->Wait();
            const auto            previous = std::make_shared<NumberPayload>(13.0f);
            AssetGenerationResult result{{}, {}, previous};
            EXPECT_EQ(request->TakeResult(Context(), result).code, AssetGeneratorCode::Failed);
            EXPECT_EQ(result.payload, previous);
            EXPECT_EQ(request->Poll().status.message, "Invalid geometry");
        }

        TEST_F(AssetGeneratorRegistryTest, QueuedCancellationSkipsProviderAndCompletedCancellationBlocksApply)
        {
            ThreadPool pool(1);
            Gate       gate;
            const auto calls    = std::make_shared<std::atomic_uint32_t>(0);
            const auto provider = std::make_shared<TestGenerator>("test.generator.queued");
            provider->generate  = [state = gate.state, calls](const auto &input, auto &control) {
                calls->fetch_add(1, std::memory_order_relaxed);
                state->Run(control);
                return AssetGenerationOutput{{}, input.payload};
            };
            ASSERT_TRUE(registry->Register(owner, provider));
            const auto                              snapshot = registry->Find(provider->id);
            std::shared_ptr<AssetGenerationRequest> running;
            std::shared_ptr<AssetGenerationRequest> queued;
            ASSERT_TRUE(registry->Start(snapshot, Input(), pool, running));
            ASSERT_TRUE(gate.WaitEntered());
            ASSERT_TRUE(registry->Start(snapshot, Input(), pool, queued));
            AssetGenerationResult result;
            EXPECT_EQ(queued->TakeResult(Context(), result).code, AssetGeneratorCode::Pending);
            queued->Cancel();
            gate.Release();
            running->Wait();
            queued->Wait();
            EXPECT_EQ(calls->load(std::memory_order_relaxed), 1u);
            EXPECT_EQ(queued->Poll().state, AssetGenerationState::Cancelled);
            EXPECT_EQ(running->Poll().state, AssetGenerationState::Succeeded);
            running->Cancel();
            EXPECT_EQ(running->TakeResult(Context(), result).code, AssetGeneratorCode::Cancelled);
        }

        TEST_F(AssetGeneratorRegistryTest, ReentrantSupportWithdrawalDoesNotInvokeRemovedProvider)
        {
            const auto provider = std::make_shared<TestGenerator>("test.generator.reentrant");
            provider->support   = [&] {
                registry->UnregisterOwner(owner);
                return AssetGeneratorStatus{};
            };
            ASSERT_TRUE(registry->Register(owner, provider));
            EXPECT_TRUE(registry->List(Context()).empty());
        }
    } // namespace
} // namespace sky::editor::test
