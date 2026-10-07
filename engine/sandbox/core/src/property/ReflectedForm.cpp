//
// Created on 2026/10/04.
//

#include <algorithm>
#include <core/type/TypeInfo.h>
#include <cstring>
#include <editor/core/property/PropertyDefaults.h>
#include <editor/core/property/ReflectedForm.h>
#include <utility>

namespace sky::editor {

    namespace {

        void FixParents(PropertyField &node)
        {
            for (auto &child : node.children) {
                child.parent = &node;
                FixParents(child);
            }
        }

        bool AnyBytesEqual(const Any &a, const Any &b)
        {
            const TypeInfoRT *ia = a.Info();
            const TypeInfoRT *ib = b.Info();
            if (ia == nullptr || ib == nullptr || ia->staticInfo == nullptr || ib->staticInfo == nullptr) {
                return false;
            }
            if (a.Data() == nullptr || b.Data() == nullptr) {
                return a.Data() == b.Data();
            }
            if (ia->registeredId != ib->registeredId) {
                return false;
            }
            if (ia->registeredId == TypeInfo<std::string>::RegisteredId()) {
                const std::string *sa = a.GetAsConst<std::string>();
                const std::string *sb = b.GetAsConst<std::string>();
                return sa != nullptr && sb != nullptr && *sa == *sb;
            }
            const size_t size = ia->staticInfo->size;
            return size > 0 && std::memcmp(a.Data(), b.Data(), size) == 0;
        }

    } // namespace

    void ReflectedForm::Build(const PropertyObject &object, const PropertyEditorRegistry &inRegistry)
    {
        root     = object;
        registry = &inRegistry;
        defaults.clear();

        // Reset baseline: a default-constructed instance of the bound type. The
        // scratch form captures each field's default from it (keyed by path); the
        // live build below then reuses those values. Falls back to a snapshot of
        // the object's own values when the type has no default constructor.
        const sky::TypeInfoRT *info        = (object.type != nullptr) ? object.type->info : nullptr;
        Any                    typeDefault = MakeDefaultValue(info);
        if (typeDefault.Data() != nullptr && typeDefault.Data() != object.object) {
            ReflectedForm scratch;
            scratch.root     = PropertyObject{typeDefault.Data(), object.type};
            scratch.registry = &inRegistry;
            scratch.Rebuild();
            defaults = std::move(scratch.defaults);
        }
        Rebuild();
    }

    void ReflectedForm::Rebuild()
    {
        sections.clear();
        if (!IsValid() || registry == nullptr) {
            return;
        }

        FormSection section;
        if (root.type->info != nullptr) {
            section.title = std::string(root.type->info->name);
        }

        const Uuid ownerTypeId = (root.type->info != nullptr) ? root.type->info->registeredId : Uuid{};
        for (const auto &entry : root.type->members) {
            if (!ReadPropertyAttributes(entry.second).visible) {
                continue;
            }
            PropertyDescriptor descriptor(root.object, &entry.second, std::string(entry.first), section.title);
            PropertyField      field;
            BuildField(std::move(descriptor), *registry, std::string(entry.first), field);
            ApplyAppearance(field, ownerTypeId, std::string(entry.first));
            section.fields.push_back(std::move(field));
        }
        std::stable_sort(section.fields.begin(), section.fields.end(), [](const PropertyField &a, const PropertyField &b) {
            const int ka = a.hasOrder ? a.order : 0;
            const int kb = b.hasOrder ? b.order : 0;
            return ka < kb;
        });

        for (auto &field : section.fields) {
            FixParents(field);
            AssignDefaults(field);
        }
        sections.push_back(std::move(section));
    }

    void
    ReflectedForm::BuildField(PropertyDescriptor descriptor, const PropertyEditorRegistry &inRegistry, const std::string &path, PropertyField &out)
    {
        out.descriptor = std::move(descriptor);
        out.control    = inRegistry.Resolve(out.descriptor);
        out.kind       = out.control.kind;
        out.path       = path;
        out.label      = out.control.label.empty() ? out.descriptor.GetDisplayName() : out.control.label;

        if (out.kind == PropertyEditorKind::Sequence) {
            out.isSequence = true;
            out.ownedValue = out.descriptor.GetValue();
            std::vector<PropertyDescriptor> elements;
            out.descriptor.BuildSequenceChildren(elements);
            for (size_t i = 0; i < elements.size(); ++i) {
                PropertyField child;
                BuildField(std::move(elements[i]), inRegistry, path + "[" + std::to_string(i) + "]", child);
                out.children.push_back(std::move(child));
            }
            return;
        }

        if (out.kind == PropertyEditorKind::Struct) {
            out.isStruct   = true;
            out.ownedValue = out.descriptor.GetValue();
            if (const TypeNode *node = out.descriptor.GetStructType(); node != nullptr) {
                const Uuid ownerTypeId = (node->info != nullptr) ? node->info->registeredId : Uuid{};
                for (const auto &entry : node->members) {
                    PropertyDescriptor child(out.ownedValue.Data(), &entry.second, std::string(entry.first), out.descriptor.GetCategory());
                    PropertyField      childField;
                    BuildField(std::move(child), inRegistry, path + "." + std::string(entry.first), childField);
                    ApplyAppearance(childField, ownerTypeId, std::string(entry.first));
                    out.children.push_back(std::move(childField));
                }
                std::stable_sort(out.children.begin(), out.children.end(), [](const PropertyField &a, const PropertyField &b) {
                    const int ka = a.hasOrder ? a.order : 0;
                    const int kb = b.hasOrder ? b.order : 0;
                    return ka < kb;
                });
            }
        }
    }

    void ReflectedForm::ApplyAppearance(PropertyField &field, const Uuid &ownerTypeId, const std::string &memberName)
    {
        if (registry == nullptr) {
            return;
        }
        if (const MemberAppearance *appearance = registry->GetMemberAppearance(ownerTypeId, memberName)) {
            if (!appearance->label.empty()) {
                field.label = appearance->label;
            }
            field.order    = appearance->order;
            field.hasOrder = appearance->hasOrder;
        }
    }

    void ReflectedForm::AssignDefaults(PropertyField &node)
    {
        const auto iter = defaults.find(node.path);
        if (iter != defaults.end()) {
            node.hasDefault   = true;
            node.defaultValue = iter->second;
        } else {
            node.hasDefault   = true;
            node.defaultValue = node.descriptor.GetValue();
            defaults.emplace(node.path, node.defaultValue);
        }
        for (auto &child : node.children) {
            AssignDefaults(child);
        }
    }

    bool ReflectedForm::IsModified(const PropertyField &field) const
    {
        if (!field.hasDefault) {
            return false;
        }
        return !AnyBytesEqual(field.descriptor.GetValue(), field.defaultValue);
    }

    bool ReflectedForm::ResetToDefault(PropertyField &field, CommandService &commands)
    {
        if (!field.hasDefault) {
            return false;
        }
        return Edit(field, field.defaultValue, commands);
    }

    bool ReflectedForm::Edit(PropertyField &field, Any value, CommandService &commands)
    {
        if (!field.descriptor.IsValid()) {
            return false;
        }

        PropertyField *rootField    = &field;
        Any            newRootValue = std::move(value);

        if (field.parent != nullptr) {
            field.descriptor.SetValue(newRootValue);
            for (PropertyField *node = field.parent; node != nullptr && node->parent != nullptr; node = node->parent) {
                node->descriptor.SetValue(node->ownedValue);
            }
            rootField = &field;
            while (rootField->parent != nullptr) {
                rootField = rootField->parent;
            }
            newRootValue = rootField->ownedValue;
        }

        UndoCommandPtr command = rootField->descriptor.MakeEditCommand(std::move(newRootValue));
        if (!command) {
            return false;
        }
        commands.Execute(std::move(command));
        if (onChanged) {
            onChanged();
        }
        return true;
    }

} // namespace sky::editor
