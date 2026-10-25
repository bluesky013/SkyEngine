#pragma once

#include <editor/core/document/WorldDocument.h>
#include <editor/core/property/EditorPropertySource.h>
#include <editor/core/selection/SelectionService.h>

namespace sky::editor {
    enum class EditorSceneObject { CUBE, PLANE, POINT_LIGHT, DIRECTIONAL_LIGHT, CAMERA, ENVIRONMENT, EMPTY };
    class WorldEditorState final : public IEditorPropertySource, public IWorldEvent {
    public:
        explicit WorldEditorState(SelectionService &selection) : selection(selection)
        {
        }
        ~WorldEditorState() override;
        void                           Bind(WorldDocument *document);
        void                           Create(EditorSceneObject kind);
        void                           DeleteSelection();
        bool                           CanAddComponent(const Uuid &type) const;
        bool                           RequestAddComponent(const Uuid &type);
        void                           Tick();
        std::vector<PropertyObject>    Resolve(const Uuid &id) const override;
        std::vector<EditorOutlineItem> Outline() const override;
        void                           OnEdited(const Uuid &) override;
        UndoCommandPtr                 MakeEditCommand(const Uuid &, const PropertyDescriptor &, Any) override;
        void                           OnActorAttached(Actor *) override
        {
        }
        void OnActorDetached(Actor *) override;

    private:
        SelectionService                  &selection;
        WorldDocument                     *document = nullptr;
        WorldPtr                           world;
        std::vector<std::pair<Uuid, Uuid>> pendingComponents;
        bool                               refreshSelection = false;
    };
} // namespace sky::editor
