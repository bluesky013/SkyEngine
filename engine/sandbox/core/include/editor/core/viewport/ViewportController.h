//
// Created on 2026/10/22.
//

#pragma once

#include <array>
#include <editor/core/command/CommandService.h>
#include <editor/core/document/WorldDocument.h>
#include <editor/core/property/PropertyChangeNotifier.h>
#include <editor/core/selection/SelectionService.h>
#include <editor/core/viewport/TransformGizmo.h>
#include <memory>

namespace sky {
    class TransformComponent;
}
namespace sky::editor {
    enum class ViewportNavigation { NONE, LOOK, PAN, ORBIT };
    class ViewportController {
    public:
        ViewportController(SelectionService &selection, CommandService &commands, PropertyChangeNotifier &changes);
        ~ViewportController();
        void                    Bind(WorldDocument *document);
        void                    SetEditing(bool value);
        bool                    IsEditing() const;
        bool                    CanTransform() const;
        void                    SetTool(GizmoTool value);
        GizmoTool               GetTool() const;
        void                    ToggleSpace();
        bool                    IsLocal() const;
        void                    ToggleSnap();
        bool                    IsSnapping() const;
        void                    SetVisualProvider(std::shared_ptr<const IGizmoVisualProvider> provider);
        bool                    BuildGeometry(Vector2 extent, const GizmoStyle &style, GizmoGeometry &geometry);
        bool                    BeginDrag(GizmoHandle handle, Vector2 pointer, Vector2 extent, const GizmoStyle &style);
        void                    MovePointer(Vector2 point);
        void                    Release();
        void                    Cancel();
        bool                    HasGesture() const;
        bool                    IsNavigating() const;
        GizmoHandle             ActiveHandle() const;
        void                    BeginNavigation(ViewportNavigation kind, Vector2 point, float height);
        bool                    NavigationKey(uint32_t key, bool down);
        void                    Tick(float seconds);
        bool                    FocusSelection();
        const std::string      &Error() const;
        EditorCamera            camera;
        ViewportControlSettings settings;

    private:
        TransformComponent                         *Selected() const;
        TransformComponent                         *DragTarget() const;
        bool                                        DragValid() const;
        GizmoDrawContext                            Context(Vector2 extent, const GizmoStyle &style) const;
        SelectionService                           &selection;
        CommandService                             &commands;
        PropertyChangeNotifier                     &changes;
        SelectionService::CallbackId                selectionCallback = 0;
        CommandService::CallbackId                  historyCallback   = 0;
        WorldDocument                              *document          = nullptr;
        WorldPtr                                    world;
        std::shared_ptr<const IGizmoVisualProvider> visualProvider;
        GizmoTool                                   tool    = GizmoTool::TRANSLATE;
        bool                                        editing = true, local = false, snapping = false;
        bool                                        dragging = false;
        Uuid                                        dragId, parentId;
        Transform                                   before, lastPreview, parentBefore;
        GizmoDrag                                   drag;
        GizmoHandle                                 active     = GizmoHandle::NONE;
        ViewportNavigation                          navigation = ViewportNavigation::NONE;
        Vector2                                     previousPointer;
        float                                       viewportHeight = 1;
        std::array<bool, 256>                       keys{};
        std::string                                 error;
    };
} // namespace sky::editor
