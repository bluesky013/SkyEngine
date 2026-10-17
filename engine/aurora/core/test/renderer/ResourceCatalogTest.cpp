//
// Created on 2026/10/16.
//

#include "resource/ResourceCatalog.h"

#include "prepare/ScenePrepareScheduler.h"
#include "scene/CpuSceneStore.h"
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <limits>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {
    constexpr uint64_t        epoch = 11;
    constexpr SceneId         sceneA{0, 1};
    constexpr SceneId         sceneB{1, 1};
    constexpr RenderEntityRef entityA{sceneA, {0, 1}};
    constexpr RenderEntityRef entityB{sceneB, {0, 1}};

    RenderResourceKey Key(uint64_t revision = 7)
    {
        return {Uuid::CreateWithSeed(41), revision, 1, epoch};
    }
    ResourceSubscriber Subscriber(RenderEntityRef entity = entityA, uint64_t revision = 1, uint64_t sequence = 1)
    {
        return {entity, ResourceBindingSlot::MESH, revision, sequence};
    }
    RenderEntityDesc Desc(RenderResourceKey key = Key(), uint64_t revision = 1)
    {
        RenderEntityDesc desc;
        desc.mesh     = {key, revision};
        desc.material = {key, revision};
        return desc;
    }
    RenderResourcePayload Payload(RenderResourceKey key = Key(), size_t bytes = 16, float bounds = 7)
    {
        return {key, Name("mesh-v1"), std::vector<uint8_t>(bytes, static_cast<uint8_t>(key.revision)), {AABB{Vector3(-bounds), Vector3(bounds)}}};
    }
    class ResourceCatalogTest : public testing::Test {
    protected:
        void SetUp() override
        {
            Init({epoch});
            Apply({CreateScene{sceneA, {}}, CreateScene{sceneB, {}}, CreateRenderEntity{entityA, Desc()}, CreateRenderEntity{entityB, Desc()}});
        }
        void Init(ResourceCatalogInit info)
        {
            auto created = ResourceCatalog::Create(info);
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<ResourceCatalog>>(created));
            catalog = std::move(std::get<std::unique_ptr<ResourceCatalog>>(created));
        }
        void Apply(std::vector<RenderCommand> commands)
        {
            FramePacket packet;
            packet.sequence   = ++sequence;
            packet.commands   = std::move(commands);
            const auto result = store.Apply(packet);
            ASSERT_TRUE(result.Succeeded());
            ASSERT_TRUE(result.skipped.empty());
            for (const auto &retirement : result.retirements)
                ASSERT_TRUE(store.AcknowledgeRetirement(retirement));
        }
        ResourceRequest Request(ResourceSubscriber subscriber = Subscriber(), bool retry = false)
        {
            const auto requested = catalog->Request(store, subscriber, retry);
            EXPECT_TRUE(std::holds_alternative<ResourceRequest>(requested));
            const auto *result = std::get_if<ResourceRequest>(&requested);
            return result ? *result : ResourceRequest{};
        }
        ResourceTicket Ready(ResourceSubscriber subscriber = Subscriber(), RenderResourceKey key = Key(), float bounds = 7)
        {
            const auto request = Request(subscriber);
            auto       payload = Payload(key, 16, bounds);
            EXPECT_FALSE(catalog->Supply(request.ticket, payload));
            EXPECT_FALSE(catalog->AdoptResident(request.ticket, key, std::make_shared<ResourceBacking>()));
            return request.ticket;
        }
        ResourceCatalogStatus Status(const ResourceTicket &ticket)
        {
            const auto result = catalog->GetStatus(ticket);
            EXPECT_TRUE(std::holds_alternative<ResourceCatalogStatus>(result));
            const auto *value = std::get_if<ResourceCatalogStatus>(&result);
            return value ? *value : ResourceCatalogStatus{};
        }
        CpuSceneStore                    store;
        std::unique_ptr<ResourceCatalog> catalog;
        uint64_t                         sequence = 0;
    };
} // namespace

TEST_F(ResourceCatalogTest, OwnedPayloadRetryReplacesTheTicketAndRetainsSharedSubscribers)
{
    const auto original = Request();
    Request(Subscriber(entityB));
    ASSERT_FALSE(catalog->Fail(original.ticket, {RendererErrorCode::OUT_OF_MEMORY, "injected"}));
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(Key())));
    auto retried = catalog->Request(Key(), true);
    ASSERT_TRUE(std::holds_alternative<ResourceRequest>(retried));
    const auto ticket = std::get<ResourceRequest>(retried).ticket;
    EXPECT_NE(ticket, original.ticket);
    auto payload = Payload();
    EXPECT_TRUE(catalog->Supply(original.ticket, payload));
    EXPECT_FALSE(catalog->Supply(ticket, payload));
    EXPECT_FALSE(catalog->Cancel(Subscriber()));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::CPU_READY);
    EXPECT_FALSE(catalog->Cancel(Subscriber(entityB)));
    EXPECT_EQ(Status(ticket).state, RenderResourceState::CANCELLED);
}

TEST_F(ResourceCatalogTest, ImageLayoutRejectsMalformedOrIncompletePayloadWithoutConsumption)
{
    const auto request     = Request();
    auto       payload     = Payload(Key(), 448);
    payload.metadata.image = RenderImageLayout{9, 5, 4, 2};
    EXPECT_EQ(RenderImageByteSize(*payload.metadata.image), 448u);
    for (uint32_t invalid = 0; invalid < 8; ++invalid) {
        auto  broken = payload;
        auto &layout = *broken.metadata.image;
        if (invalid == 0)
            layout.width = 0;
        if (invalid == 1)
            layout.height = 0;
        if (invalid == 2)
            layout.mipLevels = 5;
        if (invalid == 3)
            layout.arrayLayers = 0;
        if (invalid == 4)
            layout = {UINT32_MAX, UINT32_MAX, 1, 1};
        if (invalid == 5)
            layout.format = static_cast<RenderImageFormat>(255);
        if (invalid == 6)
            broken.bytes.pop_back();
        if (invalid == 7)
            broken.bytes.push_back(0);
        const auto size = broken.bytes.size();
        EXPECT_TRUE(catalog->Supply(request.ticket, broken));
        EXPECT_EQ(broken.bytes.size(), size);
        EXPECT_EQ(Status(request.ticket).state, RenderResourceState::REQUESTED);
        EXPECT_EQ(catalog->CpuBytes(), 0u);
    }
    EXPECT_FALSE(catalog->Supply(request.ticket, payload));
    EXPECT_EQ(Status(request.ticket).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(catalog->CpuBytes(), 448u);
    EXPECT_TRUE(catalog->AdoptResident(request.ticket, Key(), std::make_shared<ResourceBacking>()));
    EXPECT_TRUE(catalog->AdoptResident(request.ticket, Key(), std::make_shared<ResourceBacking>(ResourceImageBacking{})));
    EXPECT_EQ(Status(request.ticket).state, RenderResourceState::CPU_READY);
}

TEST_F(ResourceCatalogTest, ImageCandidateDoesNotSelectBufferBackingFromSameFormatFamily)
{
    Ready();
    const auto old = catalog->Freeze(store);
    ASSERT_TRUE(old.cpu->front().selected);
    Apply({UpdateRenderEntity{entityA, Desc(Key(8), 2), 1}});
    const auto request     = Request(Subscriber(entityA, 2, 2));
    auto       payload     = Payload(Key(8), 64);
    payload.metadata.image = RenderImageLayout{4, 4};
    ASSERT_FALSE(catalog->Supply(request.ticket, payload));
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    EXPECT_EQ(old.cpu->front().selected->resource, Key());
    EXPECT_TRUE(std::holds_alternative<ResourceBufferBacking>(*old.pins.front()->backing));
}

TEST_F(ResourceCatalogTest, SharedRequestsDeduplicateAcrossScenesAndCancelIndependently)
{
    const auto a = Request();
    const auto b = Request(Subscriber(entityB));
    EXPECT_TRUE(a.needsCpuData);
    EXPECT_FALSE(b.needsCpuData);
    EXPECT_EQ(a.ticket, b.ticket);
    EXPECT_FALSE(Request().needsCpuData);
    ASSERT_FALSE(catalog->Cancel(Subscriber()));
    EXPECT_EQ(Status(a.ticket).state, RenderResourceState::REQUESTED);
    auto payload = Payload();
    ASSERT_FALSE(catalog->Supply(b.ticket, payload));
    EXPECT_TRUE(payload.bytes.empty());
    ASSERT_FALSE(catalog->AdoptResident(b.ticket, Key(), std::make_shared<ResourceBacking>()));
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.cpu->size(), 1u);
    ASSERT_EQ(frozen.pins.size(), 1u);
    EXPECT_EQ(frozen.cpu->front().subscriber.entity, entityB);
    EXPECT_EQ(frozen.cpu->front().selected, frozen.pins.front()->cpu);
    ASSERT_FALSE(catalog->Cancel(Subscriber(entityB)));
    EXPECT_EQ(Status(a.ticket).state, RenderResourceState::RESIDENT);
    EXPECT_EQ(catalog->Prune(store), 1u);
    EXPECT_EQ(frozen.cpu->front().selected->resource, Key());
}

TEST_F(ResourceCatalogTest, MeshAndMaterialUseIndependentSubscriptionSlots)
{
    const auto mesh      = Request();
    auto       material  = Subscriber();
    material.slot        = ResourceBindingSlot::MATERIAL;
    const auto requested = Request(material);
    EXPECT_EQ(mesh.ticket, requested.ticket);
    ASSERT_FALSE(catalog->Cancel(Subscriber()));
    EXPECT_EQ(Status(mesh.ticket).state, RenderResourceState::REQUESTED);
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.cpu->size(), 1u);
    EXPECT_EQ(frozen.cpu->front().subscriber.slot, ResourceBindingSlot::MATERIAL);
    EXPECT_FALSE(frozen.cpu->front().selected);
}

TEST_F(ResourceCatalogTest, FailureRequiresExplicitRetryAndRejectsLateAttempts)
{
    const auto first = Request();
    ASSERT_FALSE(catalog->Fail(first.ticket, {RendererErrorCode::NOT_READY, "decode failed"}));
    EXPECT_EQ(Status(first.ticket).error.message, "decode failed");
    EXPECT_FALSE(Request().needsCpuData);
    auto       newer    = Subscriber(entityA, 1, 2);
    const auto rejected = catalog->Request(store, newer);
    ASSERT_TRUE(std::holds_alternative<RendererError>(rejected));
    EXPECT_EQ(std::get<RendererError>(rejected).code, RendererErrorCode::NOT_READY);
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber(), true)));
    const auto retried = Request(newer, true);
    EXPECT_TRUE(retried.needsCpuData);
    EXPECT_NE(retried.ticket, first.ticket);
    auto payload = Payload();
    EXPECT_TRUE(catalog->Supply(first.ticket, payload));
    EXPECT_EQ(payload.bytes.size(), 16u);
    EXPECT_TRUE(catalog->Fail(first.ticket, {}));
    EXPECT_TRUE(catalog->AdoptResident(first.ticket, Key(), std::make_shared<ResourceBacking>()));
    EXPECT_TRUE(catalog->Cancel(Subscriber()));
    EXPECT_EQ(Status(retried.ticket).state, RenderResourceState::REQUESTED);
    ASSERT_FALSE(catalog->Supply(retried.ticket, payload));
    ASSERT_FALSE(catalog->AdoptResident(retried.ticket, Key(), std::make_shared<ResourceBacking>()));
    EXPECT_EQ(Status(retried.ticket).state, RenderResourceState::RESIDENT);
}

TEST_F(ResourceCatalogTest, CancellationReleasesCatalogBytesButKeepsBorrowedSourceAlive)
{
    const auto requested = Request();
    auto       payload   = Payload();
    ASSERT_FALSE(catalog->Supply(requested.ticket, payload));
    auto borrower = catalog->BorrowPayload(requested.ticket);
    ASSERT_TRUE(borrower);
    std::weak_ptr<const RenderResourcePayload> weak = borrower;
    EXPECT_EQ(catalog->CpuBytes(), 16u);
    ASSERT_FALSE(catalog->Cancel(Subscriber()));
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    EXPECT_EQ(borrower->bytes.front(), 7u);
    EXPECT_EQ(Status(requested.ticket).state, RenderResourceState::CANCELLED);
    EXPECT_FALSE(catalog->Cancel(Subscriber()));
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber(entityA, 1, 2))));
    const auto retried = Request(Subscriber(entityA, 1, 2), true);
    EXPECT_NE(retried.ticket, requested.ticket);
    borrower.reset();
    EXPECT_TRUE(weak.expired());
}

TEST_F(ResourceCatalogTest, SharedRetryPreservesOtherSubscribers)
{
    const auto failed = Request();
    Request(Subscriber(entityB));
    ASSERT_FALSE(catalog->Fail(failed.ticket, {RendererErrorCode::NOT_READY, "decode failed"}));
    const auto retry = Request(Subscriber(entityA, 1, 2), true);
    ASSERT_FALSE(catalog->Cancel(Subscriber(entityA, 1, 2)));
    EXPECT_EQ(Status(retry.ticket).state, RenderResourceState::REQUESTED);
    const auto other = Request(Subscriber(entityB));
    EXPECT_EQ(other.ticket, retry.ticket);
    EXPECT_FALSE(other.needsCpuData);
    auto payload = Payload();
    ASSERT_FALSE(catalog->Supply(other.ticket, payload));
    ASSERT_FALSE(catalog->AdoptResident(other.ticket, Key(), std::make_shared<ResourceBacking>()));
    EXPECT_EQ(catalog->Freeze(store).cpu->front().subscriber.entity, entityB);
}

TEST_F(ResourceCatalogTest, BindingRevisionAndSequencePreventLateRebinding)
{
    const auto a = Request();
    Request(Subscriber(entityB));
    Apply({UpdateRenderEntity{entityA, Desc(Key(8), 2), 1}});
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber())));
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber(entityA, 2, 1))));
    const auto next    = Request(Subscriber(entityA, 2, 2));
    auto       payload = Payload();
    ASSERT_FALSE(catalog->Supply(a.ticket, payload));
    ASSERT_FALSE(catalog->AdoptResident(a.ticket, Key(), std::make_shared<ResourceBacking>()));
    EXPECT_TRUE(catalog->Cancel(Subscriber()));
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.cpu->size(), 2u);
    EXPECT_EQ(frozen.cpu->front().desired, Key(8));
    EXPECT_EQ(frozen.cpu->front().subscriber.bindingRevision, 2u);
    EXPECT_EQ(Status(next.ticket).state, RenderResourceState::REQUESTED);
    EXPECT_EQ(frozen.pins.size(), 1u);
}

TEST_F(ResourceCatalogTest, ReusedEntityAndSceneGenerationsInvalidateSubscriptions)
{
    const auto old = Request();
    Apply({DestroyRenderEntity{entityA}});
    const RenderEntityRef replacement{sceneA, {0, 2}};
    Apply({CreateRenderEntity{replacement, Desc(Key(8))}});
    EXPECT_TRUE(catalog->Freeze(store).cpu->empty());
    EXPECT_EQ(Status(old.ticket).state, RenderResourceState::CANCELLED);
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber())));
    const auto next = Request(Subscriber(replacement));
    EXPECT_TRUE(next.needsCpuData);
    EXPECT_TRUE(catalog->Cancel(Subscriber()));
    Apply({DestroyScene{sceneA}});
    const SceneId         newScene{0, 2};
    const RenderEntityRef newEntity{newScene, {0, 1}};
    Apply({CreateScene{newScene, {}}, CreateRenderEntity{newEntity, Desc(Key(9))}});
    EXPECT_TRUE(catalog->Freeze(store).cpu->empty());
    EXPECT_EQ(Status(next.ticket).state, RenderResourceState::CANCELLED);
    EXPECT_TRUE(Request(Subscriber(newEntity)).needsCpuData);
    EXPECT_TRUE(catalog->Cancel(Subscriber(replacement)));
}

TEST_F(ResourceCatalogTest, StaleEcsBindingsArePrunedWithoutReceivingANewRequest)
{
    const auto requested = Request();
    auto       payload   = Payload();
    ASSERT_FALSE(catalog->Supply(requested.ticket, payload));
    Apply({UpdateRenderEntity{entityA, Desc(Key(8), 2), 1}});
    EXPECT_TRUE(catalog->Freeze(store).cpu->empty());
    EXPECT_EQ(Status(requested.ticket).state, RenderResourceState::CANCELLED);
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    EXPECT_TRUE(catalog->AdoptResident(requested.ticket, Key(), std::make_shared<ResourceBacking>()));
}

TEST_F(ResourceCatalogTest, FrozenFallbackPinsMatchingBoundsAndBackingThroughRetirement)
{
    const auto first = Ready();
    auto       old   = catalog->Freeze(store);
    ASSERT_EQ(old.pins.size(), 1u);
    std::weak_ptr<const void> weakBacking = old.pins.front()->backing;
    const auto                cpuBorrow   = old.cpu;
    auto                      gpuBorrow   = old.pins;
    Apply({UpdateRenderEntity{entityA, Desc(Key(8), 2), 1}});
    const auto next    = Request(Subscriber(entityA, 2, 2));
    auto       payload = Payload(Key(8), 16, 80);
    ASSERT_FALSE(catalog->Supply(next.ticket, payload));
    auto fallback = catalog->Freeze(store);
    ASSERT_EQ(fallback.cpu->size(), 1u);
    ASSERT_TRUE(fallback.cpu->front().selected);
    EXPECT_EQ(fallback.cpu->front().selected->resource, Key());
    EXPECT_FLOAT_EQ(fallback.cpu->front().selected->metadata.bounds->max.x, 7);
    EXPECT_EQ(fallback.cpu->front().selected, fallback.pins.front()->cpu);
    EXPECT_EQ(catalog->Prune(store), 0u);
    ASSERT_FALSE(catalog->AdoptResident(next.ticket, Key(8), std::make_shared<ResourceBacking>()));
    EXPECT_EQ(fallback.cpu->front().selected->resource, Key());
    const auto current = catalog->Freeze(store);
    ASSERT_TRUE(current.cpu->front().selected);
    EXPECT_EQ(current.cpu->front().selected->resource, Key(8));
    EXPECT_FLOAT_EQ(current.cpu->front().selected->metadata.bounds->max.x, 80);
    EXPECT_EQ(catalog->Prune(store), 1u);
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->GetStatus(first)));
    old      = {};
    fallback = {};
    EXPECT_FALSE(weakBacking.expired());
    gpuBorrow.clear();
    EXPECT_TRUE(weakBacking.expired());
    EXPECT_FLOAT_EQ(cpuBorrow->front().selected->metadata.bounds->max.x, 7);
}

TEST_F(ResourceCatalogTest, FallbackCannotCrossProductUsageOrUseAFutureRevision)
{
    Ready();
    auto other    = Key(8);
    other.product = Uuid::CreateWithSeed(42);
    Apply({UpdateRenderEntity{entityA, Desc(other, 2), 1}});
    Request(Subscriber(entityA, 2, 2));
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    other = Key(8);
    ++other.usageVariant;
    Apply({UpdateRenderEntity{entityA, Desc(other, 3), 2}});
    Request(Subscriber(entityA, 3, 3));
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    Apply({UpdateRenderEntity{entityA, Desc(Key(6), 4), 3}});
    Request(Subscriber(entityA, 4, 4));
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    auto wrongEpoch = Key(8);
    ++wrongEpoch.deviceEpoch;
    Apply({UpdateRenderEntity{entityA, Desc(wrongEpoch, 5), 4}});
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber(entityA, 5, 5))));
    EXPECT_TRUE(catalog->Freeze(store).cpu->empty());
}

TEST_F(ResourceCatalogTest, KnownIncompatibleFormatRemainsIneligibleAfterFailureAndRetry)
{
    Ready();
    Apply({UpdateRenderEntity{entityA, Desc(Key(8), 2), 1}});
    const auto next    = Request(Subscriber(entityA, 2, 2));
    auto       payload = Payload(Key(8));
    payload.format     = Name("mesh-v2");
    ASSERT_FALSE(catalog->Supply(next.ticket, payload));
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    ASSERT_FALSE(catalog->Fail(next.ticket, {RendererErrorCode::NOT_READY, "upload failed"}));
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    const auto retry = Request(Subscriber(entityA, 2, 3), true);
    EXPECT_FALSE(catalog->Freeze(store).cpu->front().selected);
    payload = Payload(Key(8));
    EXPECT_TRUE(catalog->Supply(retry.ticket, payload));
    EXPECT_EQ(payload.bytes.size(), 16u);
}

TEST_F(ResourceCatalogTest, CatalogIdentityAndDeviceEpochRejectForeignTickets)
{
    const auto first = Request();
    Init({epoch});
    const auto second = Request();
    EXPECT_NE(first.ticket, second.ticket);
    EXPECT_EQ(first.ticket.sequence, second.ticket.sequence);
    auto payload = Payload();
    EXPECT_TRUE(catalog->Supply(first.ticket, payload));
    EXPECT_TRUE(catalog->Fail(first.ticket, {}));
    Init({epoch + 1});
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber())));
    EXPECT_TRUE(catalog->Supply(second.ticket, payload));
    EXPECT_EQ(payload.bytes.size(), 16u);
}

TEST_F(ResourceCatalogTest, BudgetsRejectWithoutConsumingPayloadOrReplacingSubscription)
{
    Init({epoch, 16, 1, 1});
    const auto first = Request();
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber(entityB))));
    EXPECT_FALSE(Request().needsCpuData);
    auto       payload = Payload(Key(), 17);
    const auto full    = catalog->Supply(first.ticket, payload);
    ASSERT_TRUE(full);
    EXPECT_EQ(full->code, RendererErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(payload.bytes.size(), 17u);
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    payload.bytes.resize(16);
    ASSERT_FALSE(catalog->Supply(first.ticket, payload));
    Apply({UpdateRenderEntity{entityA, Desc(Key(8), 2), 1}});
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber(entityA, 2, 2))));
    EXPECT_EQ(Status(first.ticket).state, RenderResourceState::CPU_READY);
    EXPECT_EQ(catalog->Prune(store), 1u);
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    EXPECT_TRUE(Request(Subscriber(entityA, 2, 2)).needsCpuData);
}

TEST_F(ResourceCatalogTest, InvalidMetadataAndBackingNeverBecomeResident)
{
    EXPECT_TRUE(std::holds_alternative<RendererError>(ResourceCatalog::Create({})));
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->Request(store, Subscriber(entityA, 1, 0))));
    const auto requested = Request();
    auto       payload   = Payload(Key(8));
    EXPECT_TRUE(catalog->Supply(requested.ticket, payload));
    payload                        = Payload();
    payload.metadata.bounds->max.x = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(catalog->Supply(requested.ticket, payload));
    payload.metadata.bounds->max.x = -8;
    EXPECT_TRUE(catalog->Supply(requested.ticket, payload));
    payload        = Payload();
    payload.format = {};
    EXPECT_TRUE(catalog->Supply(requested.ticket, payload));
    EXPECT_EQ(payload.bytes.size(), 16u);
    payload.format = Name("mesh-v1");
    ASSERT_FALSE(catalog->Supply(requested.ticket, payload));
    EXPECT_TRUE(catalog->AdoptResident(requested.ticket, Key(8), std::make_shared<ResourceBacking>()));
    EXPECT_TRUE(catalog->AdoptResident(requested.ticket, Key(), {}));
    EXPECT_EQ(Status(requested.ticket).state, RenderResourceState::CPU_READY);
    ASSERT_FALSE(catalog->AdoptResident(requested.ticket, Key(), std::make_shared<ResourceBacking>()));
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    EXPECT_FALSE(catalog->BorrowPayload(requested.ticket));
    EXPECT_TRUE(catalog->Fail(requested.ticket, {}));
}

TEST_F(ResourceCatalogTest, CpuBudgetIsAggregateAndPromotionReleasesOnlyCatalogOwnership)
{
    Init({epoch, 24});
    Apply({UpdateRenderEntity{entityB, Desc(Key(8), 2), 1}});
    const auto a       = Request();
    const auto b       = Request(Subscriber(entityB, 2));
    auto       payload = Payload();
    ASSERT_FALSE(catalog->Supply(a.ticket, payload));
    auto borrower = catalog->BorrowPayload(a.ticket);
    ASSERT_TRUE(borrower);
    payload = Payload(Key(8));
    ASSERT_TRUE(catalog->Supply(b.ticket, payload));
    EXPECT_EQ(catalog->CpuBytes(), 16u);
    EXPECT_EQ(payload.bytes.size(), 16u);
    payload.bytes.resize(8);
    ASSERT_FALSE(catalog->Supply(b.ticket, payload));
    EXPECT_EQ(catalog->CpuBytes(), 24u);
    ASSERT_FALSE(catalog->AdoptResident(a.ticket, Key(), std::make_shared<ResourceBacking>()));
    EXPECT_EQ(catalog->CpuBytes(), 8u);
    EXPECT_EQ(borrower->bytes.size(), 16u);
    ASSERT_FALSE(catalog->Fail(b.ticket, {RendererErrorCode::NOT_READY, "upload failed"}));
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    const auto frozen = catalog->Freeze(store);
    ASSERT_EQ(frozen.cpu->size(), 2u);
    EXPECT_EQ(frozen.cpu->at(1).selected->resource, Key());
}

TEST_F(ResourceCatalogTest, PrepareWorkersRetainFrozenCpuVersionWhileNextVersionBecomesReady)
{
    Ready();
    auto unavailable    = Key();
    unavailable.product = Uuid::CreateWithSeed(42);
    Apply({UpdateRenderEntity{entityA, Desc(Key(8), 2), 1}, UpdateRenderEntity{entityB, Desc(unavailable, 2), 1}});
    const auto next = Request(Subscriber(entityA, 2, 2));
    Request(Subscriber(entityB, 2));
    auto payload = Payload(Key(8), 16, 80);
    ASSERT_FALSE(catalog->Supply(next.ticket, payload));
    const auto resources = catalog->Freeze(store);
    ASSERT_EQ(resources.cpu->size(), 2u);
    EXPECT_FALSE(resources.cpu->at(1).selected);
    const auto frame = store.Freeze();
    ASSERT_TRUE(frame);
    auto scheduler = ScenePrepareScheduler::Create({2});
    ASSERT_TRUE(scheduler.scheduler);
    std::promise<void> started;
    auto               entered = started.get_future();
    std::promise<void> resume;
    const auto         released = resume.get_future().share();
    SystemDesc         reader{1, "ReadFrozenMesh"};
    reader.accesses = {ComponentAccess<RenderMeshBinding>("scene", SceneAccessOwner(sceneA), AccessMode::READ)};
    float bounds    = 0;
    auto  jobs =
        scheduler.scheduler->Submit(*frame, {reader}, [cpu = resources.cpu, &started, released, &bounds](const FrozenPrepareContext &context) {
            started.set_value();
            if (released.wait_for(5s) != std::future_status::ready) {
                context.Fail("Timed out waiting for version promotion");
                return;
            }
            context.Read<RenderMeshBinding>(sceneA).ForEach([&](auto, const auto &binding) {
                EXPECT_EQ(binding.binding.resource, Key(8));
                ASSERT_EQ(cpu->front().selected->resource, Key());
                bounds = cpu->front().selected->metadata.bounds->max.x;
            });
        });
    ASSERT_TRUE(jobs.Accepted());
    EXPECT_EQ(entered.wait_for(5s), std::future_status::ready);
    EXPECT_FALSE(catalog->AdoptResident(next.ticket, Key(8), std::make_shared<ResourceBacking>()));
    resume.set_value();
    EXPECT_TRUE(jobs.group->Wait().Succeeded());
    EXPECT_FLOAT_EQ(bounds, 7);
    EXPECT_EQ(catalog->Freeze(store).cpu->front().selected->resource, Key(8));
}

TEST_F(ResourceCatalogTest, B14ReleasedExternalCandidateSurvivesOnlyItsQueuedBorrowers)
{
    const auto key     = Key(90);
    const auto request = std::get<ResourceRequest>(catalog->Request(key));
    auto       payload = Payload(key);
    ASSERT_FALSE(catalog->Supply(request.ticket, payload));
    auto frozen = catalog->Freeze(store, true, std::span{&key, 1});
    ASSERT_EQ(frozen.pins.size(), 1u);
    catalog->Release(key);
    EXPECT_EQ(catalog->Prune(store), 0u);
    frozen = {};
    EXPECT_EQ(catalog->Prune(store), 1u);
    EXPECT_EQ(catalog->CpuBytes(), 0u);
    EXPECT_TRUE(std::holds_alternative<RendererError>(catalog->GetStatus(request.ticket)));
}
