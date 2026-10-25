//
// Created on 2026/09/21.
//

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace sky::editor {

    // A toolkit-agnostic editor extension.
    //
    // Implementations register asset creators, actor creators, gizmo factories,
    // inspectors, and so on through the core singletons. No UI-toolkit or render
    // type appears in this contract.
    class EditorExtension {
    public:
        virtual ~EditorExtension() = default;

        virtual const char *GetName() const = 0;
        virtual void        Register()      = 0;
        virtual void        Unregister()    = 0;

        // Resource-relative icon directories this extension contributes (e.g. "icons/aurora").
        // The host scans each for `*.svg` and registers them by file stem. Empty by default.
        virtual std::vector<std::string> GetIconDirectories() const
        {
            return {};
        }
    };

    using EditorExtensionPtr = std::unique_ptr<EditorExtension>;
    struct EditorExtensionRegistration {
        EditorExtensionRegistration(std::shared_ptr<const void> lease, EditorExtensionPtr extension)
            : moduleLease(std::move(lease)), extension(std::move(extension)) {}
        EditorExtensionRegistration(EditorExtensionRegistration &&) noexcept = default;
        EditorExtensionRegistration &operator=(EditorExtensionRegistration value) noexcept
        {
            moduleLease.swap(value.moduleLease);
            extension.swap(value.extension);
            return *this;
        }
        EditorExtension *operator->() const { return extension.get(); }
        EditorExtension *get() const { return extension.get(); }
        std::shared_ptr<const void> moduleLease;
        EditorExtensionPtr extension;
    };

} // namespace sky::editor
