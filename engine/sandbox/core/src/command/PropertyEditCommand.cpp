//
// Created on 2026/09/21.
//

#include <editor/core/command/PropertyEditCommand.h>
#include <utility>

namespace sky::editor {

    PropertyEditCommand::PropertyEditCommand(void *object, const serialize::TypeMemberNode *member, Any value,
                                             std::string name)
        : UndoCommand(std::move(name)), object(object), member(member), newValue(std::move(value))
    {
        if (object != nullptr && member != nullptr && member->getterFn != nullptr) {
            oldValue = member->getterFn(object);
        } else if (object != nullptr && member != nullptr && member->getterConstFn != nullptr) {
            oldValue = member->getterConstFn(object);
        }
    }

    void PropertyEditCommand::Do()
    {
        if (IsValid()) {
            member->setterFn(object, newValue.Data());
            if (member->valueChangedFn != nullptr) {
                member->valueChangedFn(object);
            }
        }
    }

    void PropertyEditCommand::Undo()
    {
        if (IsValid() && static_cast<bool>(oldValue)) {
            member->setterFn(object, oldValue.Data());
            if (member->valueChangedFn != nullptr) {
                member->valueChangedFn(object);
            }
        }
    }

} // namespace sky::editor
