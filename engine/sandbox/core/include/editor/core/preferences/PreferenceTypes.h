//
// Created on 2026/10/06.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sky::editor {

    enum class PreferenceType : uint8_t {
        BOOL = 0,
        INT,
        FLOAT,
        STRING,
        COLOR,
    };

    struct PreferenceColor {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    // A typed preference value. Only the member matching `type` is meaningful.
    struct PreferenceValue {
        PreferenceType  type       = PreferenceType::BOOL;
        bool            boolValue  = false;
        int64_t         intValue   = 0;
        double          floatValue = 0.0;
        std::string     stringValue;
        PreferenceColor colorValue;

        static PreferenceValue Bool(bool value);
        static PreferenceValue Int(int64_t value);
        static PreferenceValue Float(double value);
        static PreferenceValue Str(std::string value);
        static PreferenceValue Color(const PreferenceColor &value);

        bool Equals(const PreferenceValue &other) const;
    };

    // One setting: a stable key, a display label, a typed default, and optional
    // widget hints (slider min/max; a non-empty options list renders a combo).
    struct PreferenceEntry {
        std::string     key;
        std::string     label;
        PreferenceValue defaultValue;

        double                   minValue = 0.0;
        double                   maxValue = 0.0;
        std::vector<std::string> options;
    };

    struct PreferenceSection {
        std::string                  id;
        std::string                  title;
        std::vector<PreferenceEntry> entries;
    };

    struct PreferencePage {
        std::string                    id;
        std::string                    title;
        std::vector<PreferenceSection> sections;
    };

} // namespace sky::editor
