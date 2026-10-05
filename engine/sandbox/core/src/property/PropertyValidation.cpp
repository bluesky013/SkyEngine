//
// Created on 2026/10/04.
//

#include <editor/core/property/PropertyValidation.h>
#include <core/type/TypeInfo.h>
#include <framework/serialization/SerializationContext.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace sky::editor {

    namespace {

        bool ParseDoubleFull(const std::string &text, double &out)
        {
            if (text.empty()) {
                return false;
            }
            const char *begin = text.c_str();
            char *end = nullptr;
            const double value = std::strtod(begin, &end);
            if (end == begin) {
                return false;
            }
            while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)) != 0) {
                ++end;
            }
            if (*end != '\0') {
                return false;
            }
            out = value;
            return true;
        }

        bool ContainsFraction(const std::string &text)
        {
            return text.find('.') != std::string::npos || text.find('e') != std::string::npos ||
                   text.find('E') != std::string::npos;
        }

        double ClampAndStep(const EditorControl &control, double value)
        {
            if (control.hasRange) {
                value = std::clamp(value, control.rangeMin, control.rangeMax);
                if (control.rangeStep > 0.0) {
                    value = control.rangeMin +
                            std::round((value - control.rangeMin) / control.rangeStep) * control.rangeStep;
                    value = std::clamp(value, control.rangeMin, control.rangeMax);
                }
            }
            return value;
        }

        Any MakeEnumRaw(const TypeInfoRT *info, int64_t raw)
        {
            if (info == nullptr || info->staticInfo == nullptr) {
                return {};
            }
            uint8_t buffer[8] = {0};
            const size_t size = std::min<size_t>(info->staticInfo->size, sizeof(buffer));
            std::memcpy(buffer, &raw, size);
            return Any::Create(info, buffer);
        }

        Any MakeNumber(const TypeInfoRT *info, double value)
        {
            if (info->registeredId == TypeInfo<float>::RegisteredId()) {
                return Any(static_cast<float>(value));
            }
            if (info->registeredId == TypeInfo<double>::RegisteredId()) {
                return Any(value);
            }
            if (info->registeredId == TypeInfo<int8_t>::RegisteredId()) {
                return Any(static_cast<int8_t>(value));
            }
            if (info->registeredId == TypeInfo<uint8_t>::RegisteredId()) {
                return Any(static_cast<uint8_t>(value));
            }
            if (info->registeredId == TypeInfo<int16_t>::RegisteredId()) {
                return Any(static_cast<int16_t>(value));
            }
            if (info->registeredId == TypeInfo<uint16_t>::RegisteredId()) {
                return Any(static_cast<uint16_t>(value));
            }
            if (info->registeredId == TypeInfo<int32_t>::RegisteredId()) {
                return Any(static_cast<int32_t>(value));
            }
            if (info->registeredId == TypeInfo<uint32_t>::RegisteredId()) {
                return Any(static_cast<uint32_t>(value));
            }
            if (info->registeredId == TypeInfo<int64_t>::RegisteredId()) {
                return Any(static_cast<int64_t>(value));
            }
            if (info->registeredId == TypeInfo<uint64_t>::RegisteredId()) {
                return Any(static_cast<uint64_t>(value));
            }
            return {};
        }

    } // namespace

    bool IsInputCharAllowed(PropertyEditorKind kind, char c)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        switch (kind) {
        case PropertyEditorKind::Integer:
            return std::isdigit(uc) != 0 || c == '+' || c == '-';
        case PropertyEditorKind::Float:
            return std::isdigit(uc) != 0 || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E';
        case PropertyEditorKind::Bool:
        case PropertyEditorKind::Enum:
            return std::isalnum(uc) != 0 || c == '_' || c == ' ';
        case PropertyEditorKind::String:
            return std::isprint(uc) != 0;
        default:
            return false;
        }
    }

    int DecimalsForControl(const EditorControl &control)
    {
        if (control.hasRange && control.rangeStep > 0.0) {
            int decimals = 0;
            double step = control.rangeStep;
            while (step < 1.0 && decimals < 4) {
                step *= 10.0;
                ++decimals;
            }
            return decimals;
        }
        return 3;
    }

    Any ParseValueText(const TypeInfoRT *type, const EditorControl &control, const std::string &text, bool &ok)
    {
        ok = false;
        if (type == nullptr || type->staticInfo == nullptr) {
            return {};
        }

        if (type->staticInfo->isEnum) {
            for (size_t i = 0; i < control.enumNames.size() && i < control.enumValues.size(); ++i) {
                if (control.enumNames[i] == text) {
                    ok = true;
                    return MakeEnumRaw(type, control.enumValues[i]);
                }
            }
            double numeric = 0.0;
            if (ParseDoubleFull(text, numeric)) {
                ok = true;
                return MakeEnumRaw(type, static_cast<int64_t>(numeric));
            }
            return {};
        }
        if (type->registeredId == TypeInfo<bool>::RegisteredId()) {
            if (text == "true" || text == "True" || text == "1") {
                ok = true;
                return Any(true);
            }
            if (text == "false" || text == "False" || text == "0") {
                ok = true;
                return Any(false);
            }
            return {};
        }
        if (type->registeredId == TypeInfo<std::string>::RegisteredId()) {
            ok = true;
            return Any(text);
        }

        double numeric = 0.0;
        if (!ParseDoubleFull(text, numeric)) {
            return {};
        }
        if (control.kind == PropertyEditorKind::Integer && ContainsFraction(text)) {
            return {};
        }
        numeric = ClampAndStep(control, numeric);
        Any value = MakeNumber(type, numeric);
        ok = static_cast<bool>(value);
        return value;
    }

    Any ConstrainValue(const TypeInfoRT *type, const EditorControl &control, const Any &value)
    {
        if (type == nullptr || !value) {
            return {};
        }
        if (!control.hasRange) {
            return value;
        }

        double numeric = 0.0;
        if (const float *f = value.GetAsConst<float>()) {
            numeric = *f;
        } else if (const double *d = value.GetAsConst<double>()) {
            numeric = *d;
        } else if (const int32_t *i = value.GetAsConst<int32_t>()) {
            numeric = *i;
        } else if (const uint32_t *u = value.GetAsConst<uint32_t>()) {
            numeric = *u;
        } else {
            return value;
        }
        const Any constrained = MakeNumber(type, ClampAndStep(control, numeric));
        return constrained ? constrained : value;
    }

    std::string FormatPropertyValue(const PropertyDescriptor &descriptor, const EditorControl &control)
    {
        const Any value = descriptor.GetValue();
        const TypeInfoRT *info = value.Info();
        if (info == nullptr || info->staticInfo == nullptr || value.Data() == nullptr) {
            return {};
        }
        if (info->staticInfo->isEnum) {
            int64_t raw = 0;
            const size_t size = std::min<size_t>(info->staticInfo->size, sizeof(raw));
            std::memcpy(&raw, value.Data(), size);
            if (const TypeNode *node = GetTypeNode(info)) {
                const auto iter = node->enums.find(static_cast<uint64_t>(raw));
                if (iter != node->enums.end()) {
                    return std::string(iter->second);
                }
            }
            return std::to_string(raw);
        }
        if (info->registeredId == TypeInfo<bool>::RegisteredId()) {
            const bool *b = value.GetAsConst<bool>();
            return (b != nullptr && *b) ? "true" : "false";
        }
        if (info->registeredId == TypeInfo<std::string>::RegisteredId()) {
            const std::string *s = value.GetAsConst<std::string>();
            return s != nullptr ? *s : std::string();
        }
        char buffer[64] = {0};
        const int decimals = DecimalsForControl(control);
        if (const float *v = value.GetAsConst<float>()) {
            std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(*v));
        } else if (const double *v = value.GetAsConst<double>()) {
            std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, *v);
        } else if (const int32_t *v = value.GetAsConst<int32_t>()) {
            std::snprintf(buffer, sizeof(buffer), "%d", *v);
        } else if (const uint32_t *v = value.GetAsConst<uint32_t>()) {
            std::snprintf(buffer, sizeof(buffer), "%u", *v);
        } else if (const int64_t *v = value.GetAsConst<int64_t>()) {
            std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(*v));
        } else if (const uint64_t *v = value.GetAsConst<uint64_t>()) {
            std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(*v));
        } else {
            return info->name.empty() ? std::string("<value>") : std::string(info->name);
        }
        return buffer;
    }

} // namespace sky::editor
