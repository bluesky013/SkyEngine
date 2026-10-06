//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/preferences/PreferenceRegistry.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Holds the committed settings (what subsystems read) and a working copy the
    // dialog edits. Defaults come from the registry. UI-free.
    class PreferenceStore {
    public:
        explicit PreferenceStore(const PreferenceRegistry *registry = nullptr);
        ~PreferenceStore() = default;

        void SetRegistry(const PreferenceRegistry *registry);
        // Fills any declared key that is missing from the committed/working maps.
        void SeedDefaults();

        // Working-copy writes; validated against the declared key and type.
        bool SetBool(const std::string &key, bool value);
        bool SetInt(const std::string &key, int64_t value);
        bool SetFloat(const std::string &key, double value);
        bool SetString(const std::string &key, std::string value);
        bool SetColor(const std::string &key, const PreferenceColor &value);

        // Working-copy reads.
        bool GetBool(const std::string &key, bool &out) const;
        bool GetInt(const std::string &key, int64_t &out) const;
        bool GetFloat(const std::string &key, double &out) const;
        bool GetString(const std::string &key, std::string &out) const;
        bool GetColor(const std::string &key, PreferenceColor &out) const;

        const PreferenceValue *FindWorking(const std::string &key) const;
        const PreferenceValue *FindCommitted(const std::string &key) const;

        bool IsDirty() const
        {
            return dirty;
        }
        void MarkDirty()
        {
            dirty = true;
        }

        void                     Commit(); // working -> committed
        void                     Revert(); // committed -> working
        void                     ResetAllToDefaults();
        bool                     ResetPageToDefaults(const std::string &pageId);
        std::vector<std::string> GetChangedKeys() const;

        std::string ToJson(int indent = 2) const;
        bool        FromJson(const std::string &json);

    private:
        const PreferenceEntry *FindDeclared(const std::string &key) const;
        bool                   Assign(const std::string &key, PreferenceType type, PreferenceValue value);
        void                   UpdateDirty();

        const PreferenceRegistry                        *registry = nullptr;
        std::unordered_map<std::string, PreferenceValue> committed;
        std::unordered_map<std::string, PreferenceValue> working;
        bool                                             dirty = false;
    };

} // namespace sky::editor
