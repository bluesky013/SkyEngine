//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/command/UndoCommand.h>
#include <memory>
#include <string>
#include <vector>

namespace sky {
    class Actor;
    class ComponentBase;
    class Any;
} // namespace sky

namespace sky::editor {
    class PropertyDescriptor;
    enum class EditorEditState { Unhandled, Ready, Error };
    struct EditorEditResult {
        EditorEditState state = EditorEditState::Unhandled;
        UndoCommandPtr  command;
        std::string     error;
    };

    // Session extensions contribute registrations and owned edits without UI or render types.
    class EditorExtension {
    public:
        virtual ~EditorExtension() = default;

        virtual const char *GetName() const = 0;
        virtual void        Register()      = 0;
        virtual void        Unregister()    = 0;

        // Preparation must not mutate the live actor; Ready owns a complete undoable edit.
        virtual EditorEditResult MakePropertyEdit(Actor &, const PropertyDescriptor &, const Any &)
        {
            return {};
        }
        // Only the detached component may be prepared; the command runs after attachment.
        virtual EditorEditResult PrepareComponentAddition(Actor &, ComponentBase &)
        {
            return {};
        }

        // Resource-relative icon directories this extension contributes (e.g. "icons/aurora").
        // The host scans each for `*.svg` and registers them by file stem. Empty by default.
        virtual std::vector<std::string> GetIconDirectories() const
        {
            return {};
        }
    };

    using EditorExtensionPtr = std::unique_ptr<EditorExtension>;
    struct EditorExtensionRegistration {
        explicit EditorExtensionRegistration(EditorExtensionPtr extension) : extension(std::move(extension))
        {
        }
        EditorExtensionRegistration(EditorExtensionRegistration &&) noexcept = default;
        EditorExtensionRegistration &operator=(EditorExtensionRegistration value) noexcept
        {
            extension.swap(value.extension);
            return *this;
        }
        EditorExtension *operator->() const
        {
            return extension.get();
        }
        EditorExtension *get() const
        {
            return extension.get();
        }
        EditorExtensionPtr extension;
    };

} // namespace sky::editor
