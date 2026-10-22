//
// Created on 2026/10/22.
//

#include <editor/core/viewport/ViewportController.h>

#include <algorithm>
#include <cmath>
#include <core/logger/Logger.h>
#include <framework/world/TransformComponent.h>

namespace sky::editor {
    namespace {
        bool Same(const Transform &a, const Transform &b)
        {
            const float sign          = a.rotation.Dot(b.rotation) < 0.f ? -1.f : 1.f;
            float       rotationDelta = 0.f;
            for (size_t i = 0; i < 4; ++i) {
                const float delta = a.rotation.v[i] - sign * b.rotation.v[i];
                rotationDelta += delta * delta;
            }
            return (a.translation - b.translation).Length() < 1.e-6f && (a.scale - b.scale).Length() < 1.e-6f && rotationDelta < 1.e-12f;
        }
        class TransformCommand final : public UndoCommand {
        public:
            TransformCommand(WorldDocument &document, Uuid id, Transform before, Transform after, PropertyChangeNotifier &changes)
                : UndoCommand("Transform object")
                , document(document)
                , world(document.GetWorld())
                , id(id)
                , before(before)
                , after(after)
                , changes(changes)
            {
            }
            void Do() override
            {
                Apply(after);
            }
            void Undo() override
            {
                Apply(before);
            }

        private:
            void Apply(const Transform &value)
            {
                auto *actor     = world->GetActorByUuid(id);
                auto *transform = actor ? actor->GetComponent<TransformComponent>() : nullptr;
                if (!transform) {
                    LOG_E("Viewport", "Cannot apply transform history: target %s is unavailable", id.ToString().c_str());
                    return;
                }
                transform->SetLocalTransform(value);
                document.MarkDirty();
                changes.Notify();
            }
            WorldDocument          &document;
            WorldPtr                world;
            Uuid                    id;
            Transform               before, after;
            PropertyChangeNotifier &changes;
        };
    } // namespace

    ViewportController::ViewportController(SelectionService &selection, CommandService &commands, PropertyChangeNotifier &changes)
        : selection(selection), commands(commands), changes(changes), visualProvider(std::make_shared<DefaultGizmoVisualProvider>())
    {
        selectionCallback = selection.AddChangedCallback([this]() { Cancel(); });
        historyCallback   = commands.AddBeforeHistoryCallback([this]() { Cancel(); });
    }
    ViewportController::~ViewportController()
    {
        Cancel();
        selection.RemoveChangedCallback(selectionCallback);
        commands.RemoveBeforeHistoryCallback(historyCallback);
    }
    void ViewportController::Bind(WorldDocument *value)
    {
        if (value == document)
            return;
        Cancel();
        commands.Clear();
        document = value;
        world    = value ? value->GetWorld() : nullptr;
        camera   = {};
    }
    void ViewportController::SetEditing(bool value)
    {
        if (editing != value)
            Cancel();
        editing = value;
    }
    bool ViewportController::IsEditing() const
    {
        return editing && document && world;
    }
    TransformComponent *ViewportController::Selected() const
    {
        const auto &items = selection.GetSelection();
        if (!IsEditing() || items.size() != 1 || items.front().type != SelectionType::ENTITY)
            return nullptr;
        auto *actor = world->GetActorByUuid(items.front().id);
        return actor ? actor->GetComponent<TransformComponent>() : nullptr;
    }
    bool ViewportController::CanTransform() const
    {
        return Selected() != nullptr;
    }
    void ViewportController::SetTool(GizmoTool value)
    {
        Cancel();
        tool = value;
    }
    GizmoTool ViewportController::GetTool() const
    {
        return tool;
    }
    void ViewportController::ToggleSpace()
    {
        if (tool != GizmoTool::SCALE) {
            Cancel();
            local = !local;
        }
    }
    bool ViewportController::IsLocal() const
    {
        return local || tool == GizmoTool::SCALE;
    }
    void ViewportController::ToggleSnap()
    {
        Cancel();
        snapping = !snapping;
    }
    bool ViewportController::IsSnapping() const
    {
        return snapping;
    }
    void ViewportController::SetVisualProvider(std::shared_ptr<const IGizmoVisualProvider> value)
    {
        Cancel();
        visualProvider = value ? std::move(value) : std::make_shared<DefaultGizmoVisualProvider>();
    }
    GizmoDrawContext ViewportController::Context(Vector2 extent, const GizmoStyle &style) const
    {
        GizmoDrawContext context;
        context.camera = camera;
        context.extent = extent;
        context.tool   = tool;
        context.local  = IsLocal();
        context.style  = style;
        if (auto *target = Selected())
            context.world = target->GetWorldTransform();
        return context;
    }
    bool ViewportController::BuildGeometry(Vector2 extent, const GizmoStyle &style, GizmoGeometry &geometry)
    {
        geometry.clear();
        if (!CanTransform() || tool == GizmoTool::SELECT)
            return true;
        Vector2 pivot;
        if (!camera.Project(Selected()->GetWorldTransform().translation, extent, pivot))
            return true;
        const auto provider = visualProvider;
        if (!provider->Build(Context(extent, style), geometry, error) || !ValidateGizmoGeometry(geometry, error)) {
            geometry.clear();
            return false;
        }
        return true;
    }
    bool ViewportController::BeginDrag(GizmoHandle handle, Vector2 pointer, Vector2 extent, const GizmoStyle &style)
    {
        Cancel();
        auto *target = Selected();
        if (!target)
            return false;
        const auto *parent = target->GetParent();
        parentBefore       = parent ? parent->GetWorldTransform() : Transform{};
        before = lastPreview = target->GetLocalTransform();
        if (!drag.Begin(Context(extent, style), handle, pointer, before, parentBefore, error))
            return false;
        dragId   = target->GetActor()->GetUuid();
        parentId = parent ? parent->GetActor()->GetUuid() : Uuid::GetEmpty();
        dragging = true;
        active   = handle;
        return true;
    }
    TransformComponent *ViewportController::DragTarget() const
    {
        auto *actor = world ? world->GetActorByUuid(dragId) : nullptr;
        return actor ? actor->GetComponent<TransformComponent>() : nullptr;
    }
    bool ViewportController::DragValid() const
    {
        const auto *target = DragTarget();
        if (!target || !Same(target->GetLocalTransform(), lastPreview))
            return false;
        const auto *parent = target->GetParent();
        return parent ? parent->GetActor()->GetUuid() == parentId && Same(parent->GetWorldTransform(), parentBefore) : parentId == Uuid::GetEmpty();
    }
    void ViewportController::MovePointer(Vector2 point)
    {
        settings.Normalize();
        if (dragging) {
            if (!DragValid()) {
                Cancel();
                return;
            }
            Transform value;
            if (drag.Update(point, snapping, settings, value)) {
                lastPreview = value;
                DragTarget()->SetLocalTransform(value);
                changes.Notify();
            }
        } else if (navigation != ViewportNavigation::NONE) {
            const auto delta = point - previousPointer;
            if (navigation == ViewportNavigation::PAN)
                camera.Pan(delta.x, delta.y, viewportHeight);
            else
                camera.Look(delta.x, delta.y, settings.sensitivity * .01745329252f, navigation == ViewportNavigation::ORBIT);
            previousPointer = point;
        }
    }
    void ViewportController::Release()
    {
        if (!dragging || !DragValid()) {
            Cancel();
            return;
        }
        auto      *target = DragTarget();
        const auto after  = lastPreview;
        dragging          = false;
        active            = GizmoHandle::NONE;
        target->SetLocalTransform(before);
        if (!Same(before, after))
            commands.Execute(std::make_unique<TransformCommand>(*document, dragId, before, after, changes));
        else
            changes.Notify();
    }
    void ViewportController::Cancel()
    {
        const bool wasDragging = dragging;
        dragging               = false;
        active                 = GizmoHandle::NONE;
        navigation             = ViewportNavigation::NONE;
        keys.fill(false);
        if (wasDragging) {
            if (auto *target = DragTarget(); target && Same(target->GetLocalTransform(), lastPreview)) {
                target->SetLocalTransform(before);
                changes.Notify();
            }
        }
    }
    bool ViewportController::HasGesture() const
    {
        return dragging || IsNavigating();
    }
    bool ViewportController::IsNavigating() const
    {
        return navigation != ViewportNavigation::NONE;
    }
    GizmoHandle ViewportController::ActiveHandle() const
    {
        return active;
    }
    void ViewportController::BeginNavigation(ViewportNavigation kind, Vector2 point, float height)
    {
        Cancel();
        if (!IsEditing())
            return;
        navigation      = kind;
        previousPointer = point;
        viewportHeight  = height;
        if (kind == ViewportNavigation::ORBIT) {
            if (auto *target = Selected()) {
                camera.pivot         = target->GetWorldTransform().translation;
                camera.orbitDistance = std::max(camera.nearPlane * 2.f, (camera.position - camera.pivot).Length());
                const auto forward   = camera.pivot - camera.position;
                camera.yaw           = std::atan2(forward.x, -forward.z);
                camera.pitch         = std::asin(std::clamp(forward.y / camera.orbitDistance, -.999f, .999f));
            }
        }
    }
    bool ViewportController::NavigationKey(uint32_t key, bool down)
    {
        if (navigation != ViewportNavigation::LOOK || key >= keys.size())
            return false;
        if (key != 'W' && key != 'A' && key != 'S' && key != 'D' && key != 'Q' && key != 'E' && key != 0x10)
            return false;
        keys[key] = down;
        return true;
    }
    void ViewportController::Tick(float seconds)
    {
        settings.Normalize();
        if (dragging && !DragValid())
            Cancel();
        if (navigation == ViewportNavigation::LOOK) {
            camera.Fly({float(keys['D']) - float(keys['A']), float(keys['E']) - float(keys['Q']), float(keys['W']) - float(keys['S'])}, seconds,
                       settings.speed * (keys[0x10] ? settings.boost : 1.f));
        }
    }
    bool ViewportController::FocusSelection()
    {
        Cancel();
        if (auto *target = Selected()) {
            camera.Focus(target->GetWorldTransform().translation);
            return true;
        }
        return false;
    }
    const std::string &ViewportController::Error() const
    {
        return error;
    }
} // namespace sky::editor
