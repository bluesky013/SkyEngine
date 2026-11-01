//
// Created on 2026/10/30.
//

#include <recast/editor/NavigationEditorService.h>

#include <core/logger/Logger.h>
#include <editor/core/document/EditorWorldContext.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <navigation/INavigationSystem.h>
#include <navigation/NaviMeshFactory.h>

namespace sky::ai {
    namespace {
        constexpr const char *BUILD_ACTION = "navigation.build";
        constexpr const char *DEBUG_ACTION = "navigation.debug";
        constexpr const char *OVERLAY_ID   = "navigation.mesh";

        INavigationSystem *Navigation(World *world)
        {
            return world ? static_cast<INavigationSystem *>(world->GetSubSystem(Name(INavigationSystem::NAME.data()))) : nullptr;
        }
    } // namespace

    bool NavigationEditorService::Register(std::shared_ptr<const void> lease)
    {
        const auto self = weak_from_this().lock();
        if (!active || registered || !lease || !self || !editor::SceneOverlayRegistry::Get()->Add(OVERLAY_ID, self, lease))
            return false;
        const auto           weak = weak_from_this();
        editor::EditorAction build;
        build.moduleLease = lease;
        build.id          = BUILD_ACTION;
        build.label       = "Build Navigation";
        build.icon        = "navigation-build";
        build.group       = editor::EditorGroup::Navigation;
        build.menu        = editor::EditorMenu::Edit;
        build.submenu     = "Navigation";
        build.enabled     = [weak] {
            const auto service = weak.lock();
            return service && service->CanBuild();
        };
        build.invoke = [weak] {
            if (const auto service = weak.lock())
                service->RequestBuild();
        };
        editor::EditorActionRegistry::Get()->Add(std::move(build));
        editor::EditorAction debug;
        debug.moduleLease = std::move(lease);
        debug.id          = DEBUG_ACTION;
        debug.label       = "Show Navigation Mesh";
        debug.icon        = "navigation-mesh";
        debug.group       = editor::EditorGroup::Navigation;
        debug.menu        = editor::EditorMenu::View;
        debug.submenu     = "Navigation";
        debug.order       = 1;
        debug.checkable   = true;
        debug.checked     = [weak] {
            const auto service = weak.lock();
            return service && service->debugVisible;
        };
        debug.enabled = [] { return Navigation(editor::EditorWorldContext::Get()->GetWorld()) != nullptr; };
        debug.invoke  = [weak] {
            if (const auto service = weak.lock())
                service->debugVisible = !service->debugVisible;
        };
        editor::EditorActionRegistry::Get()->Add(std::move(debug));
        registered = true;
        return true;
    }

    void NavigationEditorService::Unregister()
    {
        if (!registered)
            return;
        editor::EditorActionRegistry::Get()->Remove("global", BUILD_ACTION);
        editor::EditorActionRegistry::Get()->Remove("global", DEBUG_ACTION);
        editor::SceneOverlayRegistry::Get()->Remove(OVERLAY_ID);
        requestedWorld = nullptr;
        registered = debugVisible = false;
    }

    void NavigationEditorService::Deactivate()
    {
        Unregister();
        active = false;
    }

    bool NavigationEditorService::CanBuild() const
    {
        const auto *context    = editor::EditorWorldContext::Get();
        const auto *navigation = Navigation(context->GetWorld());
        return registered && context->IsEditable() && navigation && navigation->GetNaviMesh() && !requestedWorld && !generator;
    }

    void NavigationEditorService::RequestBuild()
    {
        if (CanBuild()) {
            error.clear();
            requestedWorld = editor::EditorWorldContext::Get()->GetWorld();
        }
    }

    void NavigationEditorService::Tick()
    {
        if (generator && !generator->IsWorking()) {
            if (generator->HasSucceeded()) {
                if (auto *navigation = Navigation(buildWorld.Get()))
                    navigation->OnNavMeshChanged();
                debugVisible = true;
                error.clear();
            } else {
                error = generator->GetError();
                LOG_E("NavigationEditor", "Build failed: %s", error.c_str());
            }
            generator  = nullptr;
            buildWorld = nullptr;
        }
        if (!registered || !requestedWorld)
            return;
        const auto *context = editor::EditorWorldContext::Get();
        if (!context->IsEditable() || context->GetWorld() != requestedWorld.Get()) {
            requestedWorld = nullptr;
            return;
        }
        bool preparing = false;
        for (const auto &actor : requestedWorld->GetActors()) {
            for (const auto &[type, component] : actor->GetComponents()) {
                std::string componentError;
                const auto  state = component->PollWorldPreparation(componentError);
                if (state == ComponentPreparation::Failed) {
                    error = "Actor '" + actor->GetName() + "', component " + type.ToString() + ": " +
                            (componentError.empty() ? "World preparation failed" : componentError);
                    LOG_E("NavigationEditor", "%s", error.c_str());
                    requestedWorld = nullptr;
                    return;
                }
                preparing |= state == ComponentPreparation::Preparing;
            }
        }
        if (preparing)
            return;
        generator  = NaviMeshFactory::Get()->CreateGenerator();
        buildWorld = std::move(requestedWorld);
        if (!generator) {
            error = "Navigation generator is unavailable";
            LOG_E("NavigationEditor", "%s", error.c_str());
            buildWorld = nullptr;
            return;
        }
        generator->Setup(buildWorld);
        generator->StartAsync();
    }

    void NavigationEditorService::Append(aurora::FramePacket &packet, const editor::SceneOverlayContext &context)
    {
        const auto *navigation = Navigation(context.world);
        if (!debugVisible || !navigation || !navigation->GetNaviMesh() || !context.resources) {
            render.Clear(packet);
            return;
        }
        NaviDebugGeometry geometry;
        navigation->GetNaviMesh()->BuildDebugGeometry(geometry);
        if (!render.Append(packet, context.entity, context.info, geometry, *context.resources) && !render.GetError().empty() &&
            error != render.GetError()) {
            error = render.GetError();
            LOG_W("NavigationEditor", "%s", error.c_str());
        }
    }

    void NavigationEditorService::Retire(aurora::FramePacket &packet)
    {
        render.Clear(packet);
    }
    void NavigationEditorService::OnEvent(const aurora::RendererEvent &event)
    {
        render.OnEvent(event);
    }
    void NavigationEditorService::OnRendererStopped()
    {
        render.OnRendererStopped();
    }

} // namespace sky::ai
