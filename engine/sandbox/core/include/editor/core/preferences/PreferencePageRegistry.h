//
// PreferencePageRegistry: process-wide (cross-DLL) list of Preferences pages. Each page owns a
// reflected settings object + its TypeNode; the Preferences dialog renders it with ReflectedFormView.
// Modules contribute pages (dynamic list); the fields within a page are a fixed reflected type —
// see docs/editor/preferences-and-settings.md.
//

#pragma once

#include <core/environment/Singleton.h>

#include <string>
#include <vector>

namespace sky {
    struct TypeNode;
} // namespace sky

namespace sky::editor {

    struct PreferencePage {
        std::string          id;
        std::string          title;
        void                *object = nullptr; // points at the page's reflected settings struct
        const sky::TypeNode *type   = nullptr;
    };

    class PreferencePageRegistry : public sky::Singleton<PreferencePageRegistry> {
        friend class sky::Singleton<PreferencePageRegistry>;

    public:
        void Add(PreferencePage page)
        {
            pages.push_back(std::move(page));
        }
        void Clear()
        {
            pages.clear();
        }
        const std::vector<PreferencePage> &GetPages() const
        {
            return pages;
        }
        bool IsEmpty() const
        {
            return pages.empty();
        }

    private:
        std::vector<PreferencePage> pages;
    };

} // namespace sky::editor
