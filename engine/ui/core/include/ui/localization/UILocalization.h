//
// Created on 2026/09/19.
//

#pragma once

#include <core/environment/Singleton.h>

#include <string>
#include <unordered_map>

namespace sky::ui {

    // Locale -> key -> value string tables. Singleton so plugins/game code and
    // widgets share one instance through the environment.
    class UILocalization : public Singleton<UILocalization> {
    public:
        UILocalization() = default;
        ~UILocalization() override = default;

        void SetLocale(const std::string &value) { locale = value; }
        const std::string &GetLocale() const { return locale; }

        void AddString(const std::string &localeName, const std::string &key, const std::string &value);
        void ClearLocale(const std::string &localeName);

        // Returns the value for the key in the current locale, or the key itself.
        std::string Translate(const std::string &key) const;
        bool HasKey(const std::string &key) const;

    private:
        std::string locale = "en";
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> tables;
    };

} // namespace sky::ui
