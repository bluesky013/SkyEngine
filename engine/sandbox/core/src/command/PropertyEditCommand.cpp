//
// Created on 2026/09/21.
//

#include <editor/core/command/PropertyEditCommand.h>
#include <utility>

namespace sky::editor {

    PropertyEditCommand::PropertyEditCommand(void *object, const serialize::TypeMemberNode *member, Any value, std::string name)
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
        TryDo();
    }

    void PropertyEditCommand::Undo()
    {
        TryUndo();
    }

    PropertyEditCommand::PropertyEditCommand(std::function<void *()> resolve, const serialize::TypeMemberNode *field, Any value, std::string name)
        : PropertyEditCommand(resolve ? resolve() : nullptr, field, std::move(value), std::move(name))
    {
        resolver = std::move(resolve);
        object   = nullptr;
    }

    bool PropertyEditCommand::Apply(const Any &value)
    {
        void *target = Resolve();
        if (!target || !member || !member->setterFn || !value || !value.Info() || value.Info()->registeredId != member->info->registeredId ||
            !member->setterFn(target, value.Data())) {
            return false;
        }
        if (member->valueChangedFn) {
            member->valueChangedFn(target);
        }
        return true;
    }

    bool PropertyEditCommand::TryDo()
    {
        return Apply(newValue);
    }

    bool PropertyEditCommand::TryUndo()
    {
        return Apply(oldValue);
    }

} // namespace sky::editor
