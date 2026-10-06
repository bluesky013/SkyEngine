//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/preferences/PreferenceTypes.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Registry of preference pages contributed by modules. Pages are kept in
    // registration order so the dialog category list is stable. UI-free.
    class PreferenceRegistry {
    public:
        PreferenceRegistry()  = default;
        ~PreferenceRegistry() = default;

        PreferenceRegistry(const PreferenceRegistry &)            = delete;
        PreferenceRegistry &operator=(const PreferenceRegistry &) = delete;
        PreferenceRegistry(PreferenceRegistry &&)                 = default;
        PreferenceRegistry &operator=(PreferenceRegistry &&)      = default;

        void RegisterPage(PreferencePage page);
        bool UnregisterPage(const std::string &id);
        void Clear();

        const PreferencePage              *FindPage(const std::string &id) const;
        const std::vector<PreferencePage> &GetPages() const
        {
            return pages;
        }
        bool IsEmpty() const
        {
            return pages.empty();
        }

        // Finds a declared entry by key; optionally reports its owning page id.
        const PreferenceEntry *FindEntry(const std::string &key, std::string *pageId = nullptr) const;

    private:
        std::vector<PreferencePage>             pages;
        std::unordered_map<std::string, size_t> pageIndex;
    };

} // namespace sky::editor
