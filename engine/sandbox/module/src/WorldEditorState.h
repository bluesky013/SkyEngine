#pragma once

#include <editor/core/document/WorldDocument.h>
#include <editor/core/property/EditorPropertySource.h>
#include <editor/core/selection/SelectionService.h>

namespace sky::editor {
    class EditorExtensionHost;
    std::vector<std::string> RegisterBuiltinActorCreators();
    class WorldEditorState   final : public IEditorPropertySource, public IWorldEvent {
    public:
        explicit WorldEditorState(SelectionService &selection, EditorExtensionHost *extensions = nullptr)
            : selection(selection), extensions(extensions)
        {
        }
        ~WorldEditorState() override;
        void                           Bind(WorldDocument *document);
        bool                           Create(const std::string &creatorId);
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
        EditorExtensionHost               *extensions = nullptr;
        WorldDocument                     *document   = nullptr;
        WorldPtr                           world;
        std::vector<std::pair<Uuid, Uuid>> pendingComponents;
        bool                               refreshSelection = false;
    };
} // namespace sky::editor
