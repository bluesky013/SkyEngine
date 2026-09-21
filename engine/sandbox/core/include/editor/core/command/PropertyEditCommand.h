//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/command/UndoCommand.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationFactory.h>
#include <string>

namespace sky::editor {

    // Generic reflection-driven property edit.
    //
    // The previous value is captured from the reflection getter when the command
    // is created, so it can be reverted without any per-type command code. The
    // caller owns the object and must keep the member node (registry-owned) and
    // the object alive for the command's lifetime.
    class PropertyEditCommand : public UndoCommand {
    public:
        PropertyEditCommand(void *object, const serialize::TypeMemberNode *member, Any value, std::string name = {});
        ~PropertyEditCommand() override = default;

        void Do() override;
        void Undo() override;

        bool IsValid() const { return object != nullptr && member != nullptr && member->setterFn != nullptr; }

    private:
        void *object = nullptr;
        const serialize::TypeMemberNode *member = nullptr;
        Any newValue;
        Any oldValue;
    };

} // namespace sky::editor
