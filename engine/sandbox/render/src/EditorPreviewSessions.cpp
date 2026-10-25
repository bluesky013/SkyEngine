#include "EditorPreviewSessions.h"

#include <algorithm>
#include <cmath>

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
    EditorPreviewSessions::Session *EditorPreviewSessions::Find(AssetPreviewHandle handle)
    {
        const auto found = std::ranges::find_if(sessions, [&](const auto &session) { return session->handle == handle; });
        return found == sessions.end() ? nullptr : found->get();
    }
    AssetPreviewResult EditorPreviewSessions::CreateSession(AssetPreviewScene input)
    {
        if (stopped || !resources)
            return AssetPreviewError{AssetPreviewErrorCode::Unavailable, "Preview renderer unavailable"};
        if (!input.world || !input.moduleLease || !ValidCamera(input.camera, input.width, input.height))
            return AssetPreviewError{AssetPreviewErrorCode::InvalidArgument, "Preview requires an owned World, code lease, valid camera and extent"};
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
        if (const auto snapshot = session.bridge->GeometrySnapshot())
            for (const auto &instance : snapshot->instances)
                if (instance.bounds)
                    result.bounds.push_back(AABB::Transform(*instance.bounds, instance.world));
        return result;
    }
    AssetPreviewStatusResult EditorPreviewSessions::Update(AssetPreviewHandle handle, EditorCamera camera)
    {
        auto *session = Find(handle);
        if (!session || session->closing)
            return Stale();
        if (!ValidCamera(camera, session->input.width, session->input.height))
            return AssetPreviewError{AssetPreviewErrorCode::InvalidArgument, "Invalid preview camera"};
        session->input.camera = camera;
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
            ViewDesc       view;
            view.scene          = {session.view.index, session.view.generation};
            view.output         = session.output;
            view.cameraPosition = session.input.camera.position;
            view.view           = session.input.camera.View();
            view.projection     = session.input.camera.Projection(float(extent.width) / extent.height);
            if (!session.created) {
                if (const auto error = session.scene.Create(packet, view.scene, {Name("asset-preview")}))
                    return error;
                packet.commands.push_back(CreateViewport{session.output, {ViewportKind::OFFSCREEN, extent}});
                packet.commands.push_back(CreateView{session.view, view});
                session.created         = true;
                session.createdSequence = packet.sequence;
                session.submitted       = extent;
            } else {
                if (extent.width != session.submitted.width || extent.height != session.submitted.height) {
                    packet.commands.push_back(ResizeViewport{session.output, extent, ++session.outputRevision});
                    session.submitted = extent;
                }
                packet.commands.push_back(UpdateView{session.view, view, ++session.viewRevision});
            }
            if (!textures.UpdateView(session.texture, {session.output, session.outputRevision}))
                return RendererError{RendererErrorCode::INTERNAL_ERROR, "Preview texture registration was lost"};
            if (const auto error = session.bridge->Append(packet, view.scene, session.output))
                return error;
        }
        RetireClosed();
        return {};
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
