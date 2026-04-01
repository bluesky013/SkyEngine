//
// Created by blues on 2025/5/25.
//

#pragma once

#include <core/console/ConsoleTypes.h>
#include <charconv>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>

namespace sky {

    class CommandRegistry;

    class ICVar {
    public:
        ICVar() = default;
        virtual ~ICVar() = default;

        virtual std::string_view GetName() const = 0;
        virtual std::string_view GetDesc() const = 0;
        virtual std::string_view GetCategory() const = 0;
        virtual std::string_view GetTypeName() const = 0;
        virtual CVarFlags GetFlags() const = 0;

        virtual std::string ToString() const = 0;
        virtual bool SetFromString(std::string_view str) = 0;
        virtual void ResetToDefault() = 0;
    };

    // ---- type trait helpers ----
    namespace detail {

        template <typename T>
        constexpr std::string_view CVarTypeName()
        {
            if constexpr (std::is_same_v<T, bool>)        return "bool";
            else if constexpr (std::is_same_v<T, int>)    return "int";
            else if constexpr (std::is_same_v<T, float>)  return "float";
            else if constexpr (std::is_same_v<T, double>) return "double";
            else if constexpr (std::is_same_v<T, std::string>) return "string";
            else return "unknown";
        }

        // --- ToString helpers ---
        template <typename T>
        std::string CVarToString(const T &value)
        {
            if constexpr (std::is_same_v<T, bool>) {
                return value ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else {
                // int, float, double
                return std::to_string(value);
            }
        }

        // --- FromString helpers ---
        inline bool ParseBool(std::string_view str, bool &out)
        {
            // lower-case comparison
            std::string lower(str);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lower == "true" || lower == "1" || lower == "on") {
                out = true;
                return true;
            }
            if (lower == "false" || lower == "0" || lower == "off") {
                out = false;
                return true;
            }
            return false;
        }

        template <typename T>
        bool ParseFloating(std::string_view str, T &out)
        {
            std::string buffer(str);
            char *end = nullptr;
            errno = 0;

            if constexpr (std::is_same_v<T, float>) {
                out = std::strtof(buffer.c_str(), &end);
            } else {
                out = std::strtod(buffer.c_str(), &end);
            }

            return errno == 0 && end == buffer.c_str() + buffer.size();
        }

        template <typename T>
        bool CVarFromString(std::string_view str, T &out)
        {
            if constexpr (std::is_same_v<T, bool>) {
                return ParseBool(str, out);
            } else if constexpr (std::is_same_v<T, std::string>) {
                out = std::string(str);
                return true;
            } else if constexpr (std::is_same_v<T, float>) {
                return ParseFloating(str, out);
            } else if constexpr (std::is_same_v<T, double>) {
                return ParseFloating(str, out);
            } else {
                // int
                auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out);
                return ec == std::errc{} && ptr == str.data() + str.size();
            }
        }

        inline std::string_view ExtractCategory(std::string_view name)
        {
            auto pos = name.find('.');
            return (pos != std::string_view::npos) ? name.substr(0, pos) : std::string_view{};
        }

    } // namespace detail

    template <typename T>
    class CVar : public ICVar {
    public:
        CVar(std::string_view name, const T &defaultVal, std::string_view desc, CVarFlags flags = CVarFlags::NONE);
        ~CVar() override;

        CVar(const CVar &) = delete;
        CVar &operator=(const CVar &) = delete;

        const T &Get() const { return value_; }

        bool Set(const T &newVal);

        // ICVar interface
        std::string_view GetName() const override { return name_; }
        std::string_view GetDesc() const override { return desc_; }
        std::string_view GetCategory() const override { return category_; }
        std::string_view GetTypeName() const override { return detail::CVarTypeName<T>(); }
        CVarFlags GetFlags() const override { return flags_; }

        std::string ToString() const override { return detail::CVarToString(value_); }

        bool SetFromString(std::string_view str) override
        {
            if (HasFlag(flags_, CVarFlags::READ_ONLY)) {
                return false;
            }
            T parsed{};
            if (!detail::CVarFromString<T>(str, parsed)) {
                return false;
            }
            return Set(parsed);
        }

        void ResetToDefault() override
        {
            Set(default_);
        }

        // onChange callback -- invoked when value changes
        std::function<void(const T &oldVal, const T &newVal)> onChange;

    private:
        std::string name_;
        std::string desc_;
        std::string_view category_;
        CVarFlags   flags_;
        T           value_;
        T           default_;
    };

    // Implementation of Set -- must be after class definition
    template <typename T>
    bool CVar<T>::Set(const T &newVal)
    {
        if (HasFlag(flags_, CVarFlags::READ_ONLY)) {
            return false;
        }
        if (value_ == newVal) {
            return true; // no change, no callback
        }
        T oldVal = value_;
        value_ = newVal;
        if (onChange) {
            onChange(oldVal, value_);
        }
        return true;
    }

} // namespace sky
