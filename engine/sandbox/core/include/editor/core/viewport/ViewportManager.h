//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/viewport/Viewport.h>
#include <cstddef>
#include <string>
#include <vector>

namespace sky::editor {

    // Toolkit/render-independent set of editor viewports.
    //
    // Owns viewport descriptions (id/title/content/interaction/presentation/
    // overlays); the render side creates the content targets and presentations
    // (TEXTURE/WINDOW) for each. The manager itself has no render dependency.
    class ViewportManager {
    public:
        ViewportManager() = default;
        ~ViewportManager() = default;

        ViewportManager(const ViewportManager &) = delete;
        ViewportManager &operator=(const ViewportManager &) = delete;

        bool Create(ViewportDescriptor descriptor);
        bool Destroy(const std::string &id);
        void DestroyAll();

        ViewportDescriptor *Find(const std::string &id);
        const ViewportDescriptor *Find(const std::string &id) const;

        bool SetPresentation(const std::string &id, ViewPresentation presentation);
        bool SetSize(const std::string &id, uint32_t width, uint32_t height);

        const std::vector<ViewportDescriptor> &GetAll() const { return viewports; }
        size_t GetCount() const { return viewports.size(); }

    private:
        std::vector<ViewportDescriptor> viewports;
    };

} // namespace sky::editor
