//
// Created on 2026/10/06.
//

#include <editor/core/preferences/PreferenceRegistry.h>

#include <algorithm>

namespace sky::editor {

    void PreferenceRegistry::RegisterPage(PreferencePage page)
    {
        const auto it = pageIndex.find(page.id);
        if (it != pageIndex.end()) {
            pages[it->second] = std::move(page);
            return;
        }
        pageIndex.emplace(page.id, pages.size());
        pages.push_back(std::move(page));
    }

    bool PreferenceRegistry::UnregisterPage(const std::string &id)
    {
        const auto it = pageIndex.find(id);
        if (it == pageIndex.end()) {
            return false;
        }
        pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(it->second));
        pageIndex.clear();
        for (std::size_t i = 0; i < pages.size(); ++i) {
            pageIndex.emplace(pages[i].id, i);
        }
        return true;
    }

    void PreferenceRegistry::Clear()
    {
        pages.clear();
        pageIndex.clear();
    }

    const PreferencePage *PreferenceRegistry::FindPage(const std::string &id) const
    {
        const auto it = pageIndex.find(id);
        return it != pageIndex.end() ? &pages[it->second] : nullptr;
    }

    const PreferenceEntry *PreferenceRegistry::FindEntry(const std::string &key, std::string *pageId) const
    {
        for (const PreferencePage &page : pages) {
            for (const PreferenceSection &section : page.sections) {
                for (const PreferenceEntry &entry : section.entries) {
                    if (entry.key == key) {
                        if (pageId != nullptr) {
                            *pageId = page.id;
                        }
                        return &entry;
                    }
                }
            }
        }
        return nullptr;
    }

} // namespace sky::editor
