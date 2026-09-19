//
// Created on 2026/09/19.
//

#include <ui/localization/UILocalization.h>

namespace sky::ui {

    void UILocalization::AddString(const std::string &localeName, const std::string &key, const std::string &value)
    {
        tables[localeName][key] = value;
    }

    void UILocalization::ClearLocale(const std::string &localeName)
    {
        tables.erase(localeName);
    }

    std::string UILocalization::Translate(const std::string &key) const
    {
        const auto table = tables.find(locale);
        if (table != tables.end()) {
            const auto entry = table->second.find(key);
            if (entry != table->second.end()) {
                return entry->second;
            }
        }
        return key;
    }

    bool UILocalization::HasKey(const std::string &key) const
    {
        const auto table = tables.find(locale);
        return table != tables.end() && table->second.find(key) != table->second.end();
    }

} // namespace sky::ui
