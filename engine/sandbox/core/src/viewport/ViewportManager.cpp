//
// Created on 2026/09/21.
//

#include <editor/core/viewport/ViewportManager.h>
#include <algorithm>
#include <utility>

namespace sky::editor {

    bool ViewportManager::Create(ViewportDescriptor descriptor)
    {
        if (descriptor.id.empty()) {
            return false;
        }
        auto it = std::find_if(viewports.begin(), viewports.end(),
                               [&](const ViewportDescriptor &v) { return v.id == descriptor.id; });
        if (it != viewports.end()) {
            *it = std::move(descriptor);
            return true;
        }
        viewports.push_back(std::move(descriptor));
        return true;
    }

    bool ViewportManager::Destroy(const std::string &id)
    {
        auto it = std::find_if(viewports.begin(), viewports.end(),
                               [&](const ViewportDescriptor &v) { return v.id == id; });
        if (it == viewports.end()) {
            return false;
        }
        viewports.erase(it);
        return true;
    }

    void ViewportManager::DestroyAll()
    {
        viewports.clear();
    }

    ViewportDescriptor *ViewportManager::Find(const std::string &id)
    {
        auto it = std::find_if(viewports.begin(), viewports.end(),
                               [&](const ViewportDescriptor &v) { return v.id == id; });
        return it == viewports.end() ? nullptr : &*it;
    }

    const ViewportDescriptor *ViewportManager::Find(const std::string &id) const
    {
        auto it = std::find_if(viewports.begin(), viewports.end(),
                               [&](const ViewportDescriptor &v) { return v.id == id; });
        return it == viewports.end() ? nullptr : &*it;
    }

    bool ViewportManager::SetPresentation(const std::string &id, ViewPresentation presentation)
    {
        ViewportDescriptor *viewport = Find(id);
        if (viewport == nullptr) {
            return false;
        }
        viewport->presentation = presentation;
        return true;
    }

    bool ViewportManager::SetSize(const std::string &id, uint32_t width, uint32_t height)
    {
        ViewportDescriptor *viewport = Find(id);
        if (viewport == nullptr) {
            return false;
        }
        viewport->width  = width;
        viewport->height = height;
        return true;
    }

} // namespace sky::editor
