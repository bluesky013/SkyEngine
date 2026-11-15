#include "EditorPreviewSessions.h"

#include <algorithm>
#include <array>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/PointLightComponent.h>
#include <aurora/adaptor/components/SpotLightComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <cmath>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

namespace sky::editor {
    using namespace aurora;
    namespace {
        AssetPreviewError Stale()
        {
            return {AssetPreviewErrorCode::StaleSession, "Preview session is closed or belongs to another host"};
        }
        bool ValidExtent(uint32_t width, uint32_t height)
        {
            return width && height && width <= 4096 && height <= 4096;
        }
        bool ValidBounds(const AABB &bounds)
        {
            for (uint32_t axis = 0; axis < 3; ++axis)
                if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]) || bounds.min[axis] > bounds.max[axis])
                    return false;
            return true;
        }
        std::optional<AABB> SubjectBounds(const RenderSceneBridge &bridge)
        {
            const auto          snapshot = bridge.GeometrySnapshot();
            std::optional<AABB> result;
            if (snapshot)
                for (const auto &instance : snapshot->instances) {
                    if (!instance.bounds)
                        return {};
                    const auto bounds = AABB::Transform(*instance.bounds, instance.world);
                    if (!ValidBounds(bounds))
                        return {};
                    if (result)
                        result->Merge(bounds);
                    else
                        result = bounds;
                }
            return result;
        }
        std::unique_ptr<Actor> CreateGround(const AABB &bounds, const Uuid &meshId, const Uuid &materialId)
        {
            const Vector3 extent = bounds.max - bounds.min;
            const float   size   = std::max({extent.x, extent.y, extent.z, .01f});
            const float   height = bounds.min.y - size * .001f;
            if (!std::isfinite(size) || !std::isfinite(height) || !TypeInfo<TransformComponent>::RegisteredId() ||
                !TypeInfo<StaticMeshComponent>::RegisteredId())
                return {};
            auto  actor     = std::make_unique<Actor>(Uuid::Create());
            auto *transform = actor->AddComponent<TransformComponent>();
            auto *mesh      = actor->AddComponent<StaticMeshComponent>();
            if (!transform || !mesh)
                return {};
            const auto center = bounds.min * .5f + bounds.max * .5f;
            Transform  placement;
            placement.translation = {center.x, height, center.z};
            placement.scale       = {size, 1.f, size};
            transform->SetLocalTransform(placement);
            if (!meshId || !materialId)
                return {};
            mesh->SetMeshUuid(meshId);
            mesh->SetMaterialUuid(materialId);
            mesh->SetCastShadow(false);
            mesh->SetReceiveShadow(true);
            return actor;
        }
        bool HasSceneLight(const World &world)
        {
            for (const auto &actor : world.GetActors())
                for (const auto &[type, component] : actor->GetComponents())
                    if (type == TypeInfo<DirectLightComponent>::RegisteredId() || type == TypeInfo<PointLightComponent>::RegisteredId() ||
                        type == TypeInfo<SpotLightComponent>::RegisteredId())
                        return true;
            return false;
        }
        std::unique_ptr<Actor> CreateLight(const Quaternion &rotation, float intensity, bool shadows)
        {
            if (!TypeInfo<TransformComponent>::RegisteredId() || !TypeInfo<DirectLightComponent>::RegisteredId())
                return {};
            auto  actor     = std::make_unique<Actor>(Uuid::Create());
            auto *transform = actor->AddComponent<TransformComponent>();
            auto *light     = actor->AddComponent<DirectLightComponent>();
            if (!transform || !light)
                return {};
            transform->SetLocalRotation(rotation);
            light->SetIntensity(intensity);
            light->SetCastShadow(shadows);
            return actor;
        }
        bool ValidCamera(const EditorCamera &camera, uint32_t width, uint32_t height)
        {
            const bool valid = std::isfinite(camera.position.x) && std::isfinite(camera.position.y) && std::isfinite(camera.position.z) &&
                               std::isfinite(camera.yaw) && std::isfinite(camera.pitch) && std::isfinite(camera.verticalFov) &&
                               std::isfinite(camera.nearPlane) && std::isfinite(camera.farPlane) && camera.verticalFov > 0 &&
                               camera.verticalFov < 3.14159f && camera.nearPlane > 0 && camera.farPlane > camera.nearPlane;
            if (!valid || !ValidExtent(width, height))
                return false;
            const auto view = camera.View(), projection = camera.Projection(float(width) / height);
            return std::ranges::all_of(view.v, [](float v) { return std::isfinite(v); }) &&
                   std::ranges::all_of(projection.v, [](float v) { return std::isfinite(v); });
        }
    } // namespace
    EditorPreviewSessions::EditorPreviewSessions(RendererInfo                          info,
                                                 std::shared_ptr<ISceneResourceSource> resources,
                                                 UiSnapshotProducer                   &textures,
                                                 std::function<ViewportId()>           allocate)
        : info(info), resources(std::move(resources)), textures(textures), allocate(std::move(allocate))
    {
    }
    bool EditorPreviewSessions::SetResources(std::shared_ptr<ISceneResourceSource> source)
    {
        if (stopped || !source || !sessions.empty())
            return false;
        resources = std::move(source);
        return true;
    }
    bool EditorPreviewSessions::SetGroundAssets(const Uuid &mesh, const Uuid &material)
    {
        if (stopped || !sessions.empty() || !mesh || !material)
            return false;
        groundMesh     = mesh;
        groundMaterial = material;
        return true;
    }
    EditorPreviewSessions::Session *EditorPreviewSessions::Find(AssetPreviewHandle handle)
    {
        const auto found = std::ranges::find_if(sessions, [&](const auto &session) { return session->handle == handle; });
        return found == sessions.end() ? nullptr : found->get();
    }
    AssetPreviewResult EditorPreviewSessions::CreateSession(AssetPreviewScene input)
    {
        if (stopped || !resources)
            return AssetPreviewError{AssetPreviewErrorCode::Unavailable, "Preview renderer unavailable"};
        if (!input.world || !ValidCamera(input.camera, input.width, input.height))
            return AssetPreviewError{AssetPreviewErrorCode::InvalidArgument, "Preview requires an owned World, valid camera and extent"};
        if (input.subjectBounds && !ValidBounds(*input.subjectBounds))
            return AssetPreviewError{AssetPreviewErrorCode::InvalidArgument, "Preview subject bounds must be finite and ordered"};
        if (sessions.size() >= 16 || nextScene == UINT32_MAX)
            return AssetPreviewError{AssetPreviewErrorCode::Capacity, "Preview session capacity exceeded"};
        const auto output = allocate ? allocate() : ViewportId{};
        if (!output.generation)
            return AssetPreviewError{AssetPreviewErrorCode::Capacity, "Preview output IDs exhausted"};
        const auto texture = textures.RegisterView({output, 0});
        if (!texture)
            return AssetPreviewError{AssetPreviewErrorCode::Capacity, "Preview texture IDs exhausted"};
        auto session     = std::make_unique<Session>();
        session->input   = std::move(input);
        session->handle  = {Uuid::Create(), generation};
        session->output  = output;
        session->view    = {nextScene++, 1};
        session->texture = texture;
        session->bridge  = std::make_unique<RenderSceneBridge>(info, resources);
        session->bridge->SetWorld(session->input.world.Get());
        session->bridge->EnableGeometrySnapshots(true);
        const auto handle = session->handle;
        sessions.push_back(std::move(session));
        return handle;
    }
    AssetPreviewStatusResult EditorPreviewSessions::Status(AssetPreviewHandle handle) const
    {
        if (handle.generation != generation)
            return Stale();
        if (std::ranges::find(retired, handle) != retired.end())
            return AssetPreviewStatus{AssetPreviewState::Closed};
        const auto found = std::ranges::find_if(sessions, [&](const auto &session) { return session->handle == handle; });
        if (found == sessions.end())
            return Stale();
        const auto        &session = **found;
        AssetPreviewStatus result;
        result.texture  = session.closing ? 0 : session.texture;
        result.revision = session.outputRevision;
        result.state    = session.closing                                                   ? AssetPreviewState::Closing
                          : session.created && completedSequence >= session.createdSequence ? AssetPreviewState::Ready
                                                                                            : AssetPreviewState::Pending;
        if (!session.error.empty()) {
            result.state      = AssetPreviewState::Failed;
            result.diagnostic = session.error;
        } else if (!session.bridge->GetIssues().empty()) {
            result.diagnostic = session.bridge->GetIssues().front().message;
        }
        if (const auto snapshot = session.bridge->GeometrySnapshot()) {
            for (const auto &instance : snapshot->instances) {
                if (instance.bounds)
                    result.bounds.push_back(AABB::Transform(*instance.bounds, instance.world));
                if (instance.state == SceneGeometryState::PENDING && !instance.resources && result.diagnostic.empty())
                    result.diagnostic = "Preparing mesh geometry and materials...";
            }
        }
        return result;
    }
    AssetPreviewStatusResult EditorPreviewSessions::Update(AssetPreviewHandle handle, EditorCamera camera)
    {
        auto *session = Find(handle);
        if (!session || session->closing)
            return Stale();
        const auto &previous = session->input.camera;
        if (camera.position.x == previous.position.x && camera.position.y == previous.position.y && camera.position.z == previous.position.z &&
            camera.yaw == previous.yaw && camera.pitch == previous.pitch && camera.verticalFov == previous.verticalFov &&
            camera.nearPlane == previous.nearPlane && camera.farPlane == previous.farPlane) {
            session->input.camera = camera;
            return Status(handle);
        }
        if (!ValidCamera(camera, session->input.width, session->input.height))
            return AssetPreviewError{AssetPreviewErrorCode::InvalidArgument, "Invalid preview camera"};
        session->input.camera  = camera;
        session->cameraChanged = true;
        return Status(handle);
    }
    AssetPreviewStatusResult EditorPreviewSessions::Resize(AssetPreviewHandle handle, uint32_t width, uint32_t height)
    {
        auto *session = Find(handle);
        if (!session || session->closing)
            return Stale();
        if (!ValidCamera(session->input.camera, width, height))
            return AssetPreviewError{AssetPreviewErrorCode::InvalidArgument, "Preview extent must be within 1..4096"};
        session->input.width  = width;
        session->input.height = height;
        return Status(handle);
    }
    AssetPreviewStatusResult EditorPreviewSessions::CloseSession(AssetPreviewHandle handle)
    {
        auto *session = Find(handle);
        if (!session)
            return Status(handle);
        session->closing = true;
        textures.ReleaseView(session->texture);
        return Status(handle);
    }
    std::optional<RendererError> EditorPreviewSessions::Append(FramePacket &packet)
    {
        for (auto &owned : sessions) {
            auto &session = *owned;
            if (session.retiring)
                continue;
            if (session.closing) {
                session.retiring = true;
                if (!session.created) {
                    session.sceneRetired = session.viewRetired = session.outputRetired = true;
                    continue;
                }
                session.bridge->SetWorld(nullptr);
                if (const auto error = session.bridge->Append(packet, session.scene.GetSceneId(), session.output))
                    return error;
                packet.commands.push_back(DestroyView{session.view});
                packet.commands.push_back(DestroyViewport{session.output});
                if (const auto error = session.scene.Destroy(packet))
                    return error;
                continue;
            }
            const Extent2D extent{session.input.width, session.input.height};
            const SceneId  scene{session.view.index, session.view.generation};
            const bool     resized = extent.width != session.submitted.width || extent.height != session.submitted.height;
            if (!session.created || resized || session.cameraChanged) {
                ViewDesc view;
                view.scene          = scene;
                view.output         = session.output;
                view.cameraPosition = session.input.camera.position;
                view.view           = session.input.camera.View();
                view.projection     = session.input.camera.Projection(float(extent.width) / extent.height);
                if (!session.created) {
                    if (const auto error = session.scene.Create(packet, scene, {Name("asset-preview")}))
                        return error;
                    packet.commands.push_back(CreateViewport{session.output, {ViewportKind::OFFSCREEN, extent}});
                    packet.commands.push_back(CreateView{session.view, view});
                    session.created         = true;
                    session.createdSequence = packet.sequence;
                } else {
                    if (resized) {
                        packet.commands.push_back(ResizeViewport{session.output, extent, ++session.outputRevision});
                        if (!textures.UpdateView(session.texture, {session.output, session.outputRevision}))
                            return RendererError{RendererErrorCode::INTERNAL_ERROR, "Preview texture registration was lost"};
                    }
                    packet.commands.push_back(UpdateView{session.view, view, ++session.viewRevision});
                }
                session.cameraChanged = false;
                session.submitted     = extent;
            }
            if (session.input.showGround && !session.ground && session.error.empty()) {
                const auto bounds = session.input.subjectBounds ? session.input.subjectBounds : SubjectBounds(*session.bridge);
                if (bounds) {
                    session.ground = CreateGround(*bounds, groundMesh, groundMaterial);
                    if (!session.ground)
                        session.error = "Could not create the preview ground from the subject bounds";
                }
            }
            std::array<Actor *, 3> stage{};
            size_t                 count = 0;
            if (session.ground)
                stage[count++] = session.ground.get();
            if ((session.ground || !session.input.world->GetActors().empty()) && !HasSceneLight(*session.input.world) && session.error.empty()) {
                if (!session.keyLight) {
                    session.keyLight  = CreateLight(Quaternion(-.55f, {0, 1, 0}) * Quaternion(-.65f, {1, 0, 0}), 1.25f, true);
                    session.fillLight = CreateLight(Quaternion(2.2f, {0, 1, 0}) * Quaternion(-.25f, {1, 0, 0}), .65f, false);
                }
                if (session.keyLight && session.fillLight) {
                    stage[count++] = session.keyLight.get();
                    stage[count++] = session.fillLight.get();
                } else
                    session.error = "Could not create the preview lights";
            }
            if (const auto error = session.bridge->Append(packet, scene, session.output, std::span(stage).first(count)))
                return error;
        }
        RetireClosed();
        return {};
    }
    void EditorPreviewSessions::CollectExtensionViews(std::vector<SceneExtensionSourceView> &views) const
    {
        for (const auto &session : sessions)
            if (session->created && !session->closing && !session->retiring)
                views.push_back({session->input.world->GetRuntimeId(), session->output, session->input.camera.position});
    }
    void EditorPreviewSessions::RetireClosed()
    {
        std::erase_if(sessions, [&](const auto &session) {
            if (!session->sceneRetired || !session->viewRetired || !session->outputRetired)
                return false;
            retired.push_back(session->handle);
            if (retired.size() > 64)
                retired.pop_front();
            return true;
        });
    }
    void EditorPreviewSessions::OnEvent(const RendererEvent &event)
    {
        if (const auto *completed = std::get_if<FrameCompleted>(&event))
            completedSequence = std::max(completedSequence, completed->sequence);
        for (auto &session : sessions) {
            const auto scene = SceneId{session->view.index, session->view.generation};
            session->scene.OnEvent(event);
            session->bridge->OnEvent(event);
            if (const auto *destroyed = std::get_if<SceneDestroyed>(&event); destroyed && destroyed->scene == scene)
                session->sceneRetired = true;
            if (const auto *destroyed = std::get_if<ViewDestroyed>(&event); destroyed && destroyed->view == session->view)
                session->viewRetired = true;
            if (const auto *destroyed = std::get_if<ViewportDestroyed>(&event); destroyed && destroyed->viewport == session->output)
                session->outputRetired = true;
            if (const auto *failure = std::get_if<ViewportFailed>(&event); failure && failure->viewport == session->output)
                session->error = failure->error.message;
            if (const auto *failure = std::get_if<ScenePrepareFailed>(&event); failure && failure->scene == scene)
                session->error = failure->error.message;
        }
        RetireClosed();
    }
    void EditorPreviewSessions::OnRendererStopped()
    {
        stopped = true;
        for (const auto &session : sessions) {
            textures.ReleaseView(session->texture);
            retired.push_back(session->handle);
        }
        sessions.clear();
        while (retired.size() > 64)
            retired.pop_front();
    }
} // namespace sky::editor
