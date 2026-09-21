//
// Created on 2026/09/21.
//

#include <editor/core/property/PropertyDescriptor.h>
#include <core/type/TypeInfo.h>
#include <editor/core/command/PropertyEditCommand.h>
#include <framework/serialization/SerializationContext.h>
#include <utility>

namespace sky::editor {

    PropertyDescriptor::PropertyDescriptor(void *object, const serialize::TypeMemberNode *member, std::string name,
                                           std::string category)
        : object(object), member(member), type(member != nullptr ? member->info : nullptr), elementIndex(-1),
          name(std::move(name)), displayName(this->name), category(std::move(category))
    {
        displayName = this->name;
    }

    PropertyDescriptor::PropertyDescriptor(void *object, const serialize::TypeMemberNode *member, int32_t elementIndex,
                                           std::string name, std::string category)
        : object(object), member(member), type(member != nullptr ? member->info : nullptr), elementIndex(elementIndex),
          name(std::move(name)), displayName(this->name), category(std::move(category))
    {
        displayName = this->name;
    }

    Any PropertyDescriptor::GetRawValue() const
    {
        if (!IsValid()) {
            return {};
        }
        if (member->getterFn != nullptr) {
            return member->getterFn(object);
        }
        if (member->getterConstFn != nullptr) {
            return member->getterConstFn(object);
        }
        return {};
    }

    void *PropertyDescriptor::GetSequenceElementPtr(Any &container, uint32_t index) const
    {
        if (member == nullptr || member->info == nullptr || member->info->containerInfo == nullptr) {
            return nullptr;
        }
        auto *seq = member->info->containerInfo->sequenceView;
        if (seq == nullptr || index >= seq->Count(container.Data())) {
            return nullptr;
        }
        return seq->GetByIndex(container.Data(), index);
    }

    Any PropertyDescriptor::GetValue() const
    {
        if (!IsValid()) {
            return {};
        }

        if (IsSequenceElement()) {
            Any container = GetRawValue();
            void *elementPtr = GetSequenceElementPtr(container, static_cast<uint32_t>(elementIndex));
            const TypeNode *elementType = GetElementType();
            if (elementPtr == nullptr || elementType == nullptr || elementType->info == nullptr) {
                return {};
            }
            return Any::Create(elementType->info, elementPtr);
        }

        return GetRawValue();
    }

    bool PropertyDescriptor::SetValue(const Any &value) const
    {
        if (!CanEdit()) {
            return false;
        }
        if (!member->setterFn(object, value.Data())) {
            return false;
        }
        if (member->valueChangedFn != nullptr) {
            member->valueChangedFn(object);
        }
        return true;
    }

    bool PropertyDescriptor::IsSequence() const
    {
        if (member == nullptr || member->info == nullptr || member->info->containerInfo == nullptr) {
            return false;
        }
        return member->info->containerInfo->sequenceView != nullptr &&
               member->info->containerInfo->valueType != TypeInfo<char>::RegisteredId();
    }

    uint32_t PropertyDescriptor::GetSequenceCount() const
    {
        if (IsSequenceElement() || !IsSequence()) {
            return 0;
        }
        Any container = GetRawValue();
        return static_cast<uint32_t>(member->info->containerInfo->sequenceView->Count(container.Data()));
    }

    const TypeNode *PropertyDescriptor::GetElementType() const
    {
        if (member == nullptr || member->info == nullptr || member->info->containerInfo == nullptr) {
            return nullptr;
        }
        return GetTypeNode(member->info->containerInfo->valueType);
    }

    void PropertyDescriptor::BuildSequenceChildren(std::vector<PropertyDescriptor> &out) const
    {
        if (IsSequenceElement() || !IsSequence()) {
            return;
        }

        const uint32_t count = GetSequenceCount();
        out.reserve(out.size() + count);
        for (uint32_t i = 0; i < count; ++i) {
            out.emplace_back(
                PropertyDescriptor(object, member, static_cast<int32_t>(i), name + "[" + std::to_string(i) + "]", category));
        }
    }

    const TypeNode *PropertyDescriptor::GetStructType() const
    {
        if (IsSequenceElement() || member == nullptr || member->info == nullptr) {
            return nullptr;
        }
        const TypeNode *node = GetTypeNode(member->info);
        if (node == nullptr || node->members.empty()) {
            return nullptr;
        }
        return node;
    }

    bool PropertyDescriptor::IsStruct() const
    {
        return GetStructType() != nullptr;
    }

    UndoCommandPtr PropertyDescriptor::MakeEditCommand(Any value) const
    {
        if (!CanEdit()) {
            return nullptr;
        }
        return std::make_unique<PropertyEditCommand>(object, member, std::move(value), name);
    }

    UndoCommandPtr PropertyDescriptor::MakeAddSequenceElementCommand() const
    {
        if (!CanEdit() || !IsSequence()) {
            return nullptr;
        }
        Any container = GetRawValue();
        auto *seq = member->info->containerInfo->sequenceView;
        if (seq == nullptr) {
            return nullptr;
        }
        seq->Emplace(container.Data());
        return std::make_unique<PropertyEditCommand>(object, member, std::move(container), name + " add");
    }

    UndoCommandPtr PropertyDescriptor::MakeRemoveSequenceElementCommand(uint32_t index) const
    {
        if (!CanEdit() || !IsSequence() || index >= GetSequenceCount()) {
            return nullptr;
        }
        Any container = GetRawValue();
        auto *seq = member->info->containerInfo->sequenceView;
        if (seq == nullptr) {
            return nullptr;
        }
        seq->EraseByIndex(container.Data(), index);
        return std::make_unique<PropertyEditCommand>(object, member, std::move(container), name + " remove");
    }

} // namespace sky::editor
