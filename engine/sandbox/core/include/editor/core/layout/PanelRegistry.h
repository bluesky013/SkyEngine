//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace sky::editor {

    // UI-independent panel identity. A frontend subclasses this to render the
    // panel; the core only knows the id.
    class IEditorPanel {
    public:
        virtual ~IEditorPanel() = default;
        virtual const std::string &GetPanelId() const = 0;
    };

    using PanelFactory = std::function<std::unique_ptr<IEditorPanel>()>;

    struct PanelInfo {
        std::string id;
        std::string title;
        float minWidth = 0.f;
        float minHeight = 0.f;
        PanelFactory factory;
    };

    // Registry of panels known to the editor. Layouts refer to panels by id.
    class PanelRegistry {
    public:
        PanelRegistry() = default;
        ~PanelRegistry() = default;

        PanelRegistry(const PanelRegistry &) = delete;
        PanelRegistry &operator=(const PanelRegistry &) = delete;

        void Register(PanelInfo info);
        bool Unregister(const std::string &id);

        const PanelInfo *Find(const std::string &id) const;
        bool Contains(const std::string &id) const { return Find(id) != nullptr; }

        const std::unordered_map<std::string, PanelInfo> &GetAll() const { return panels; }

    private:
        std::unordered_map<std::string, PanelInfo> panels;
    };

} // namespace sky::editor
