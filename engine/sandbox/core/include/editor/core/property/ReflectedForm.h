//
// Created on 2026/10/04.
//

#pragma once

#include <editor/core/command/CommandService.h>
#include <editor/core/property/PropertyEditor.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // One reflected member in a form, recursive over structs and sequences.
    struct PropertyField {
        PropertyDescriptor descriptor;
        PropertyEditorKind kind = PropertyEditorKind::Unknown;
        EditorControl      control;
        std::string        label;
        std::string        path;
        PropertyField     *parent = nullptr;
        std::vector<PropertyField> children;
        Any                ownedValue; // struct/sequence backing for children
        Any                defaultValue;
        bool               hasDefault = false;
        bool               isStruct = false;
        bool               isSequence = false;
        bool               expanded = false;
        int                order = 0;
        bool               hasOrder = false;
    };

    struct FormSection {
        std::string              title;
        std::vector<PropertyField> fields;
    };

    // Layout-neutral reflection-driven form: sections of recursive fields.
    class ReflectedForm {
    public:
        ReflectedForm() = default;
        ~ReflectedForm() = default;

        void Build(const PropertyObject &object, const PropertyEditorRegistry &registry);
        void Rebuild();

        bool Edit(PropertyField &field, Any value, CommandService &commands);

        // True if the field differs from the default captured when this form was
        // first bound; ResetToDefault writes it back as one undoable edit.
        bool IsModified(const PropertyField &field) const;
        bool ResetToDefault(PropertyField &field, CommandService &commands);

        std::vector<FormSection> &GetSections() { return sections; }
        const std::vector<FormSection> &GetSections() const { return sections; }
        bool IsValid() const { return root.object != nullptr && root.type != nullptr; }

    private:
        void BuildField(PropertyDescriptor descriptor, const PropertyEditorRegistry &registry,
                        const std::string &path, PropertyField &out);
        void ApplyAppearance(PropertyField &field, const Uuid &ownerTypeId, const std::string &memberName);
        void AssignDefaults(PropertyField &node);

        PropertyObject                 root;
        const PropertyEditorRegistry  *registry = nullptr;
        std::vector<FormSection>       sections;
        std::unordered_map<std::string, Any> defaults;
        bool                           defaultsCaptured = false;
    };

} // namespace sky::editor
