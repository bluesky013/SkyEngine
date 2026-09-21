//
// Created on 2026/09/21.
//

#pragma once

#include <memory>

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
        virtual void Register() = 0;
        virtual void Unregister() = 0;
    };

    using EditorExtensionPtr = std::unique_ptr<EditorExtension>;

} // namespace sky::editor
