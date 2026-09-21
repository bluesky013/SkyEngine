//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/command/UndoCommand.h>
#include <cstdint>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationFactory.h>
#include <string>
#include <vector>

namespace sky {
    struct TypeInfoRT;
    struct TypeNode;
} // namespace sky

namespace sky::editor {

    // An object instance plus its reflected type; the unit a PropertyModel inspects.
    struct PropertyObject {
        void *object = nullptr;
        const TypeNode *type = nullptr;
    };

    // A single editable reflected member, independent of any view.
    //
    // A top-level descriptor exposes a member of an object. A sequence-element
    // descriptor (elementIndex >= 0) refers to one element of a sequence member
    // and is read-only; sequence contents are edited as a whole through the
    // container member's setter.
    class PropertyDescriptor {
    public:
        PropertyDescriptor() = default;
        PropertyDescriptor(void *object, const serialize::TypeMemberNode *member, std::string name, std::string category);

        const std::string &GetName() const { return name; }
        const std::string &GetDisplayName() const { return displayName; }
        const std::string &GetCategory() const { return category; }
        const TypeInfoRT *GetType() const { return type; }
        const serialize::TypeMemberNode *GetMember() const { return member; }

        bool IsValid() const { return object != nullptr && member != nullptr; }
        bool IsSequenceElement() const { return elementIndex >= 0; }
        bool CanEdit() const { return IsValid() && !IsSequenceElement() && member->setterFn != nullptr; }

        Any GetValue() const;
        bool SetValue(const Any &value) const;

        bool IsSequence() const;
        uint32_t GetSequenceCount() const;
        const TypeNode *GetElementType() const;
        void BuildSequenceChildren(std::vector<PropertyDescriptor> &out) const;

        bool IsStruct() const;
        const TypeNode *GetStructType() const;

        // Creates an undoable edit command for this property.
        UndoCommandPtr MakeEditCommand(Any value) const;
        // Creates an undoable command that appends an element to a sequence member.
        UndoCommandPtr MakeAddSequenceElementCommand() const;
        // Creates an undoable command that removes a sequence element by index.
        UndoCommandPtr MakeRemoveSequenceElementCommand(uint32_t index) const;

    private:
        PropertyDescriptor(void *object, const serialize::TypeMemberNode *member, int32_t elementIndex,
                           std::string name, std::string category);

        Any GetRawValue() const;
        void *GetSequenceElementPtr(Any &container, uint32_t index) const;

        void *object = nullptr;
        const serialize::TypeMemberNode *member = nullptr;
        const TypeInfoRT *type = nullptr;
        int32_t elementIndex = -1;
        std::string name;
        std::string displayName;
        std::string category;
    };

} // namespace sky::editor
