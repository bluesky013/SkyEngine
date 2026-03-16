//
// Copyright 2024 SkyEngine. All rights reserved.
//

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstdint>

namespace sky {

    template <typename T>
    struct CmdValueTag {
        std::string defaultValue;
        bool hasDefault = false;
    };

    template <typename T>
    CmdValueTag<T> CmdValue() { return {}; }

    template <typename T>
    CmdValueTag<T> CmdValue(const T &def)
    {
        CmdValueTag<T> tag;
        tag.hasDefault = true;
        std::ostringstream os;
        os << def;
        tag.defaultValue = os.str();
        return tag;
    }

    inline CmdValueTag<std::string> CmdValue(const std::string &def)
    {
        return CmdValueTag<std::string>{def, true};
    }

    inline CmdValueTag<std::string> CmdValue(const char *def)
    {
        return CmdValueTag<std::string>{def, true};
    }

    class CmdParseResult {
    public:
        CmdParseResult() = default;

        size_t count(const std::string &name) const
        {
            auto it = values_.find(name);
            return (it != values_.end() && it->second.set) ? 1 : 0;
        }

        struct Proxy {
            const std::vector<std::string> &raw;

            template <typename T>
            T as() const;
        };

        Proxy operator[](const std::string &name) const
        {
            auto it = values_.find(name);
            if (it == values_.end()) {
                throw std::runtime_error("Option '" + name + "' not found");
            }
            return Proxy{it->second.rawValues};
        }

    private:
        friend class CmdOptions;

        struct Entry {
            bool set = false;
            std::vector<std::string> rawValues;
        };
        std::unordered_map<std::string, Entry> values_;
    };

    template <>
    inline std::string CmdParseResult::Proxy::as<std::string>() const
    {
        return raw.empty() ? std::string{} : raw.back();
    }

    template <>
    inline std::vector<std::string> CmdParseResult::Proxy::as<std::vector<std::string>>() const
    {
        return raw;
    }

    template <>
    inline uint32_t CmdParseResult::Proxy::as<uint32_t>() const
    {
        return raw.empty() ? 0u : static_cast<uint32_t>(std::stoul(raw.back()));
    }

    class CmdOptions {
    public:
        CmdOptions(const std::string &program, const std::string &description)
            : program_(program), description_(description) {}

        class OptionAdder {
        public:
            explicit OptionAdder(CmdOptions &opts) : opts_(opts) {}

            OptionAdder &operator()(const std::string &flags, const std::string &desc)
            {
                opts_.AddOpt(flags, desc, false, {}, false);
                return *this;
            }

            template <typename T>
            OptionAdder &operator()(const std::string &flags, const std::string &desc, CmdValueTag<T> tag)
            {
                opts_.AddOpt(flags, desc, true, tag.hasDefault ? tag.defaultValue : std::string{}, tag.hasDefault);
                return *this;
            }

        private:
            CmdOptions &opts_;
        };

        OptionAdder add_options() { return OptionAdder{*this}; }

        void allow_unrecognised_options() { allowUnrecognised_ = true; }

        CmdParseResult parse(int argc, const char *const *argv) const
        {
            CmdParseResult result;
            for (auto &[name, info] : optionMap_) {
                result.values_[name] = {};
            }

            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];
                std::string name;
                std::string val;
                bool hasVal = false;

                if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
                    auto eq = arg.find('=');
                    if (eq != std::string::npos) {
                        name = arg.substr(2, eq - 2);
                        val = arg.substr(eq + 1);
                        hasVal = true;
                    } else {
                        name = arg.substr(2);
                    }
                } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
                    std::string shortKey = arg.substr(1, 1);
                    auto it = shortToLong_.find(shortKey);
                    if (it != shortToLong_.end()) {
                        name = it->second;
                        if (arg.size() > 2) {
                            val = arg.substr(2);
                            hasVal = true;
                        }
                    } else if (!allowUnrecognised_) {
                        throw std::runtime_error("Unknown option: " + arg);
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }

                auto optIt = optionMap_.find(name);
                if (optIt == optionMap_.end()) {
                    if (!allowUnrecognised_) {
                        throw std::runtime_error("Unknown option: --" + name);
                    }
                    continue;
                }

                auto &entry = result.values_[name];
                entry.set = true;

                if (optIt->second.takesValue && !hasVal) {
                    if (i + 1 < argc) {
                        val = argv[++i];
                    }
                    hasVal = true;
                }

                if (hasVal) {
                    entry.rawValues.push_back(val);
                }
            }

            for (auto &[optName, info] : optionMap_) {
                auto &entry = result.values_[optName];
                if (!entry.set && info.hasDefault) {
                    entry.set = true;
                    entry.rawValues.push_back(info.defaultValue);
                }
            }

            return result;
        }

        CmdParseResult parse(int argc, char **argv) const
        {
            return parse(argc, const_cast<const char *const *>(argv));
        }

        std::string help() const
        {
            std::ostringstream os;
            os << program_ << " - " << description_ << "\n\nOptions:\n";
            for (auto &info : optionList_) {
                os << "  ";
                if (!info.shortName.empty()) {
                    os << "-" << info.shortName << ", ";
                } else {
                    os << "    ";
                }
                os << "--" << info.longName;
                if (info.takesValue) {
                    os << " <value>";
                }
                if (info.hasDefault) {
                    os << " [=" << info.defaultValue << "]";
                }
                os << "\t" << info.description << "\n";
            }
            return os.str();
        }

    private:
        void AddOpt(const std::string &flags, const std::string &desc, bool takesValue,
                    const std::string &defaultValue = {}, bool hasDefault = false)
        {
            std::string shortName, longName;
            auto comma = flags.find(',');
            if (comma != std::string::npos) {
                shortName = Trim(flags.substr(0, comma));
                longName = Trim(flags.substr(comma + 1));
            } else {
                longName = Trim(flags);
            }

            OptionInfo info{shortName, longName, desc, takesValue, defaultValue, hasDefault};
            optionMap_[longName] = info;
            optionList_.push_back(info);
            if (!shortName.empty()) {
                shortToLong_[shortName] = longName;
            }
        }

        static std::string Trim(const std::string &s)
        {
            auto start = s.find_first_not_of(' ');
            if (start == std::string::npos) return {};
            auto end = s.find_last_not_of(' ');
            return s.substr(start, end - start + 1);
        }

        struct OptionInfo {
            std::string shortName;
            std::string longName;
            std::string description;
            bool takesValue = false;
            std::string defaultValue;
            bool hasDefault = false;
        };

        std::string program_;
        std::string description_;
        std::unordered_map<std::string, OptionInfo> optionMap_;
        std::vector<OptionInfo> optionList_;
        std::unordered_map<std::string, std::string> shortToLong_;
        bool allowUnrecognised_ = false;
    };

} // namespace sky
