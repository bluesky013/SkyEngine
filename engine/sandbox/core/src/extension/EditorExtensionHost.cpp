//
// Created on 2026/09/21.
//

#include <editor/core/extension/EditorExtensionHost.h>

namespace sky::editor {
    namespace {
        template <class Prepare>
        EditorEditResult PrepareEdit(const std::vector<EditorExtensionRegistration> &extensions, Prepare prepare)
        {
            EditorEditResult result;
            for (const auto &extension : extensions) {
                auto candidate = prepare(*extension.get());
                if (candidate.state == EditorEditState::Error)
                    return {EditorEditState::Error, {}, candidate.error.empty() ? "Editor extension rejected the edit" : std::move(candidate.error)};
                if (candidate.state == EditorEditState::Unhandled)
                    continue;
                if (candidate.state != EditorEditState::Ready || !candidate.command)
                    return {EditorEditState::Error, {}, "Editor extension returned an invalid prepared edit"};
                if (result.state == EditorEditState::Ready)
                    return {EditorEditState::Error, {}, "Multiple editor extensions handle this edit"};
                result = std::move(candidate);
            }
            return result;
        }
    } // namespace

    EditorEditResult EditorExtensionHost::MakePropertyEdit(Actor &actor, const PropertyDescriptor &property, const Any &value) const
    {
        return registered ? PrepareEdit(extensions, [&](EditorExtension &extension) { return extension.MakePropertyEdit(actor, property, value); })
                          : EditorEditResult{};
    }
    EditorEditResult EditorExtensionHost::PrepareComponentAddition(Actor &actor, ComponentBase &component) const
    {
        return registered ? PrepareEdit(extensions, [&](EditorExtension &extension) { return extension.PrepareComponentAddition(actor, component); })
                          : EditorEditResult{};
    }

    EditorExtensionHost::~EditorExtensionHost()
    {
        UnregisterAll();
    }

    void EditorExtensionHost::Add(EditorExtensionPtr extension)
    {
        Add(EditorExtensionRegistration(std::move(extension)));
    }
    void EditorExtensionHost::Add(EditorExtensionRegistration extension)
    {
        if (extension.get() == nullptr) {
            return;
        }
        if (registered) {
            extension->Register();
        }
        extensions.push_back(std::move(extension));
    }

    void EditorExtensionHost::RegisterAll()
    {
        if (registered) {
            return;
        }
        for (auto &extension : extensions) {
            extension->Register();
        }
        registered = true;
    }

    void EditorExtensionHost::UnregisterAll()
    {
        if (!registered) {
            return;
        }
        for (auto it = extensions.rbegin(); it != extensions.rend(); ++it) {
            (*it)->Unregister();
        }
        registered = false;
    }

    const EditorExtension *EditorExtensionHost::Find(const std::string &name) const
    {
        for (const auto &extension : extensions) {
            if (name == extension->GetName()) {
                return extension.get();
            }
        }
        return nullptr;
    }

} // namespace sky::editor
