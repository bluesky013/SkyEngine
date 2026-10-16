//
// PreferencePageRegistry: process-wide (cross-DLL) list of Preferences pages. Each page owns a
// reflected settings object + its TypeNode; the Preferences dialog renders it with ReflectedFormView.
// Modules contribute pages (dynamic list); the fields within a page are a fixed reflected type —
// see docs/editor/preferences-and-settings.md.
//

#pragma once

#include <core/environment/Singleton.h>
#include <editor/core/preferences/PreferenceResult.h>

#include <algorithm>
#include <cstdint>
#include <functional>
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
        // Assigned by Add. A re-registration is a new object identity even at
        // the same address and with the same page/type id.
        uint64_t registration = 0;
        // Called with an owned candidate before persistence/live writes. It must
        // validate without mutating settings. Application runs after live writes
        // and must tolerate retry; arbitrary external effects cannot roll back.
        std::function<PreferenceResult(const PreferencePage &)> validate;
        std::function<PreferenceResult(const PreferencePage &)> apply;
    };

    class PreferencePageRegistry : public sky::Singleton<PreferencePageRegistry> {
        friend class sky::Singleton<PreferencePageRegistry>;

    public:
        void Add(PreferencePage page)
        {
            page.registration = nextRegistration++;
            pages.push_back(std::move(page));
        }
        bool Remove(const std::string &id)
        {
            const auto oldSize = pages.size();
            pages.erase(std::remove_if(pages.begin(), pages.end(), [&id](const auto &page) { return page.id == id; }), pages.end());
            return pages.size() != oldSize;
        }
        // Owners remove only their own registration, preserving a later page
        // with the same id when editor modules are replaced.
        bool RemoveRegistration(uint64_t registration)
        {
            const auto oldSize = pages.size();
            pages.erase(std::remove_if(pages.begin(), pages.end(), [registration](const auto &page) { return page.registration == registration; }),
                        pages.end());
            return pages.size() != oldSize;
        }
        bool Move(const std::string &id, std::size_t index)
        {
            const auto found = std::find_if(pages.begin(), pages.end(), [&id](const auto &page) { return page.id == id; });
            if (found == pages.end() || index >= pages.size()) {
                return false;
            }
            auto page = std::move(*found);
            pages.erase(found);
            pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(index), std::move(page));
            return true;
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
        uint64_t                    nextRegistration = 1;
    };

} // namespace sky::editor
