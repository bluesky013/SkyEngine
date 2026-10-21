//
// Created on 2026/10/04.
//

#pragma once

#include <editor/core/command/CommandService.h>
#include <editor/core/property/PropertyEditor.h>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // One reflected member in a form, recursive over structs and sequences.
    // Built trees use final reserved storage: children bind into ownedValue and
    // must not be structurally moved/reordered by consumers. Views may change
    // expansion/presentation state; Rebuild owns structural changes.
    struct PropertyField {
        PropertyDescriptor         descriptor;
        PropertyEditorKind         kind = PropertyEditorKind::Unknown;
        EditorControl              control;
        std::string                label;
        std::string                path;
        PropertyField             *parent = nullptr;
        std::vector<PropertyField> children;
        Any                        ownedValue; // struct/sequence backing for children
        Any                        defaultValue;
        bool                       hasDefault = false;
        bool                       isStruct   = false;
        bool                       isSequence = false;
        bool                       expanded   = false;
        int                        order      = 0;
        bool                       hasOrder   = false;
    };

    struct FormSection {
        std::string                title;
        std::vector<PropertyField> fields;
    };

    // Layout-neutral reflection-driven form: sections of recursive fields.
    class ReflectedForm {
    public:
        ReflectedForm()                                 = default;
        ~ReflectedForm()                                = default;
        ReflectedForm(const ReflectedForm &)            = delete;
        ReflectedForm &operator=(const ReflectedForm &) = delete;

        // Hosts may defer model/tree replacement until committed edits return.
        // The edit scope owns a separate counter state, never the form itself.
        bool IsEditing() const
        {
            return activeEdits->depth != 0;
        }

        // Binds to `object` and captures its reset baseline from the type's
        // default-constructed value (UE/Godot-style "reset to default"), so the
        // reset affordance persists across load/save. Falls back to a snapshot of
        // the object's own values when the type has no default constructor.
        // A non-null `baseline` overrides the reset baseline (e.g. a per-asset
        // cook preset), so "reset" restores that baseline rather than type default.
        void Build(const PropertyObject &object, const PropertyEditorRegistry &registry, const PropertyObject *baseline = nullptr);
        // Each borrowed object becomes a section; field paths and reset baselines stay separate.
        void BuildGroups(std::span<const PropertyObject> objects, const PropertyEditorRegistry &registry);
        void Rebuild();

        // Hosts may create domain commands from the top-level property value.
        // The factory is called only during editing; stored commands must own
        // their identity/data and never retain this descriptor or form fields.
        using EditCommandFactory = std::function<UndoCommandPtr(const PropertyDescriptor &, Any)>;
        void SetEditCommandFactory(EditCommandFactory factory)
        {
            editCommands = std::move(factory);
        }

        bool Edit(PropertyField &field, Any value, CommandService &commands);
        bool AddSequenceElement(PropertyField &field, CommandService &commands);
        bool RemoveSequenceElement(PropertyField &field, uint32_t index, CommandService &commands);

        // Invoked after a committed edit (e.g. so the owner can mark a document
        // dirty). ResetToDefault routes through Edit and fires it too.
        void SetOnChanged(std::function<void()> callback)
        {
            onChanged = std::move(callback);
        }
        // Fires onChanged directly for a host mutation outside Edit. Domain
        // persistence must live in the command, rather than this notification.
        void NotifyChanged() const
        {
            const auto changed = onChanged;
            if (changed) {
                changed();
            }
        }

        // True if the field differs from its type default (the reset baseline);
        // ResetToDefault writes the default back as one undoable edit.
        bool IsModified(const PropertyField &field) const;
        bool ResetToDefault(PropertyField &field, CommandService &commands);

        std::vector<FormSection> &GetSections()
        {
            return sections;
        }
        const std::vector<FormSection> &GetSections() const
        {
            return sections;
        }
        bool IsValid() const
        {
            return !roots.empty();
        }

    private:
        void AddRoot(const PropertyObject &object, const PropertyObject *baseline, std::string prefix);
        void BuildField(PropertyDescriptor descriptor, const PropertyEditorRegistry &registry, const std::string &path, PropertyField &out);
        void ApplyAppearance(PropertyField &field, const Uuid &ownerTypeId, const std::string &memberName);
        void AssignDefaults(PropertyField &node);

        struct ActiveEdits {
            uint32_t depth = 0;
        };

        struct RootBinding {
            PropertyObject object;
            std::string    prefix;
        };
        std::vector<RootBinding>             roots;
        const PropertyEditorRegistry        *registry = nullptr;
        std::vector<FormSection>             sections;
        std::unordered_map<std::string, Any> defaults; // path -> type default
        std::function<void()>                onChanged;
        EditCommandFactory                   editCommands;
        std::shared_ptr<ActiveEdits>         activeEdits = std::make_shared<ActiveEdits>();
        // Never lock/retain this token across a command: observers may rebind
        // or destroy the form synchronously during domain persistence.
        std::shared_ptr<void> bindingGeneration = std::make_shared<int>(0);
    };

} // namespace sky::editor
