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

        std::pair<int, bool> EffectiveOrder(const serialize::TypeMemberNode &member, const MemberAppearance *appearance)
        {
            if (appearance != nullptr && appearance->hasOrder) {
                return {appearance->order, true};
            }
            const auto attributes = ReadPropertyAttributes(member);
            return {attributes.order, attributes.hasOrder};
        }

        // Sort reflection metadata before constructing fields. Moving/sorting a
        // built field also moves an inline Any, invalidating child descriptors
        // bound to that Any's old address even if parent pointers are repaired.
        std::vector<const serialize::MemberMap::value_type *>
        OrderedMembers(const TypeNode &node, const PropertyEditorRegistry &registry, bool visibleOnly)
        {
            std::vector<const serialize::MemberMap::value_type *> members;
            members.reserve(node.members.size());
            for (const auto &entry : node.members) {
                if (!visibleOnly || ReadPropertyAttributes(entry.second).visible) {
                    members.push_back(&entry);
                }
            }
            const Uuid ownerTypeId = node.info != nullptr ? node.info->registeredId : Uuid{};
            const auto order       = [&](const auto *entry) {
                const auto *appearance = registry.GetMemberAppearance(ownerTypeId, std::string(entry->first));
                return EffectiveOrder(entry->second, appearance).first;
            };
            // Equal/default order retains reflection iteration order (currently
            // MemberMap's lexical order), not registration call order.
            std::stable_sort(members.begin(), members.end(), [&](const auto *a, const auto *b) { return order(a) < order(b); });
            return members;
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

    void ReflectedForm::Build(const PropertyObject &object, const PropertyEditorRegistry &inRegistry, const PropertyObject *baseline)
    {
        bindingGeneration.reset();
        root     = object;
        registry = &inRegistry;
        defaults.clear();

        // Reset baseline: an explicit baseline object when provided, else a
        // default-constructed instance of the bound type. The scratch form captures
        // each field's default from it (keyed by path); the live build below then
        // reuses those values. Falls back to a snapshot of the object's own values
        // when neither is available.
        PropertyObject base;
        Any            typeDefault;
        if (baseline != nullptr && baseline->object != nullptr && baseline->type != nullptr) {
            base = *baseline;
        } else {
            const sky::TypeInfoRT *info = (object.type != nullptr) ? object.type->info : nullptr;
            typeDefault                 = MakeDefaultValue(info);
            if (typeDefault.Data() != nullptr && typeDefault.Data() != object.object) {
                base = PropertyObject{typeDefault.Data(), object.type};
            }
        }

        if (base.object != nullptr && base.object != object.object) {
            ReflectedForm scratch;
            scratch.root     = base;
            scratch.registry = &inRegistry;
            scratch.Rebuild();
            defaults = std::move(scratch.defaults);
        }
        Rebuild();
    }

    void ReflectedForm::Rebuild()
    {
        bindingGeneration = std::make_shared<int>(0);
        sections.clear();
        if (!IsValid() || registry == nullptr) {
            return;
        }

        sections.emplace_back();
        FormSection &section = sections.back();
        if (root.type->info != nullptr) {
            section.title = std::string(root.type->info->name);
        }

        const Uuid ownerTypeId = (root.type->info != nullptr) ? root.type->info->registeredId : Uuid{};
        const auto members     = OrderedMembers(*root.type, *registry, true);
        section.fields.reserve(members.size());
        for (const auto *entry : members) {
            PropertyDescriptor descriptor(root.object, &entry->second, std::string(entry->first), section.title);
            section.fields.emplace_back();
            PropertyField &field = section.fields.back();
            BuildField(std::move(descriptor), *registry, std::string(entry->first), field);
            ApplyAppearance(field, ownerTypeId, std::string(entry->first));
        }

        for (auto &field : section.fields) {
            AssignDefaults(field);
        }
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
            out.children.reserve(elements.size());
            for (size_t i = 0; i < elements.size(); ++i) {
                out.children.emplace_back();
                PropertyField &child = out.children.back();
                child.parent         = &out;
                BuildField(std::move(elements[i]), inRegistry, path + "[" + std::to_string(i) + "]", child);
            }
            return;
        }

        if (out.kind == PropertyEditorKind::Struct) {
            out.isStruct   = true;
            out.ownedValue = out.descriptor.GetValue();
            if (const TypeNode *node = out.descriptor.GetStructType(); node != nullptr) {
                const Uuid ownerTypeId = (node->info != nullptr) ? node->info->registeredId : Uuid{};
                const auto members     = OrderedMembers(*node, inRegistry, true);
                out.children.reserve(members.size());
                for (const auto *entry : members) {
                    PropertyDescriptor child(out.ownedValue.Data(), &entry->second, std::string(entry->first), out.descriptor.GetCategory());
                    out.children.emplace_back();
                    PropertyField &childField = out.children.back();
                    childField.parent         = &out;
                    BuildField(std::move(child), inRegistry, path + "." + std::string(entry->first), childField);
                    ApplyAppearance(childField, ownerTypeId, std::string(entry->first));
                }
            }
        }
    }

    void ReflectedForm::ApplyAppearance(PropertyField &field, const Uuid &ownerTypeId, const std::string &memberName)
    {
        if (registry == nullptr) {
            return;
        }
        const auto *appearance = registry->GetMemberAppearance(ownerTypeId, memberName);
        const auto  order      = EffectiveOrder(*field.descriptor.GetMember(), appearance);
        field.order            = order.first;
        field.hasOrder         = order.second;
        if (appearance != nullptr) {
            if (!appearance->label.empty()) {
                field.label = appearance->label;
            }
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

    bool ReflectedForm::AddSequenceElement(PropertyField &field, CommandService &commands)
    {
        if (!field.descriptor.CanEdit() || !field.descriptor.IsSequence()) {
            return false;
        }
        Any         value    = field.descriptor.GetValue();
        const auto *type     = field.descriptor.GetType();
        auto       *sequence = type != nullptr && type->containerInfo != nullptr ? type->containerInfo->sequenceView : nullptr;
        if (!value || sequence == nullptr) {
            return false;
        }
        sequence->Emplace(value.Data());
        return Edit(field, std::move(value), commands);
    }

    bool ReflectedForm::RemoveSequenceElement(PropertyField &field, uint32_t index, CommandService &commands)
    {
        if (!field.descriptor.CanEdit() || !field.descriptor.IsSequence() || index >= field.descriptor.GetSequenceCount()) {
            return false;
        }
        Any         value    = field.descriptor.GetValue();
        const auto *type     = field.descriptor.GetType();
        auto       *sequence = type != nullptr && type->containerInfo != nullptr ? type->containerInfo->sequenceView : nullptr;
        if (!value || sequence == nullptr) {
            return false;
        }
        sequence->EraseByIndex(value.Data(), index);
        return Edit(field, std::move(value), commands);
    }

    bool ReflectedForm::Edit(PropertyField &field, Any value, CommandService &commands)
    {
        if (!field.descriptor.IsValid()) {
            return false;
        }

        // All state used after callbacks is copied before dispatch. The active
        // counter survives independently when an observer destroys this form;
        // the weak generation deliberately does not keep its binding alive.
        const auto                factory    = editCommands;
        const auto                changed    = onChanged;
        const std::weak_ptr<void> generation = bindingGeneration;
        struct EditScope {
            explicit EditScope(std::shared_ptr<ActiveEdits> inState) : state(std::move(inState))
            {
                ++state->depth;
            }
            ~EditScope()
            {
                --state->depth;
            }
            std::shared_ptr<ActiveEdits> state;
        } scope(activeEdits);

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

        UndoCommandPtr command =
            factory ? factory(rootField->descriptor, std::move(newRootValue)) : rootField->descriptor.MakeEditCommand(std::move(newRootValue));
        if (!command) {
            return false;
        }
        commands.Execute(std::move(command));
        if (!generation.expired() && changed) {
            changed();
        }
        return true;
    }

} // namespace sky::editor
