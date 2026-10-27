//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/command/UndoCommand.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationFactory.h>
#include <functional>
#include <string>

namespace sky::editor {

    // Movable document storage uses a resolver; raw bindings require stable object lifetime.
    class PropertyEditCommand : public UndoCommand {
    public:
        PropertyEditCommand(void *object, const serialize::TypeMemberNode *member, Any value, std::string name = {});
        PropertyEditCommand(std::function<void *()> resolver, const serialize::TypeMemberNode *member, Any value, std::string name = {});
        ~PropertyEditCommand() override = default;

        void Do() override;
        void Undo() override;
        bool TryDo() override;
        bool TryUndo() override;

        bool IsValid() const
        {
            return Resolve() != nullptr && member != nullptr && member->setterFn != nullptr;
        }

    private:
        void                            *object = nullptr;
        const serialize::TypeMemberNode *member = nullptr;
        Any                              newValue;
        Any                              oldValue;
        std::function<void *()>          resolver;
        void                            *Resolve() const
        {
            return resolver ? resolver() : object;
        }
        bool Apply(const Any &value);
    };

} // namespace sky::editor
