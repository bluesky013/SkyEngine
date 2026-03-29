//
// Created by blues on 2025/5/25.
//

#pragma once

#include <core/console/ConsoleTypes.h>
#include <core/environment/Singleton.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <functional>

namespace sky {

    class ICVar;

    struct CommandEntry {
        std::string  name;
        std::string  desc;
        std::string  category;
        CommandFunc  handler;
    };

    struct PrefixMatch {
        ICVar        *cvar = nullptr;
        CommandEntry *cmd  = nullptr;
    };

    class CommandRegistry : public Singleton<CommandRegistry> {
    public:
        void RegisterCVar(ICVar *cvar);
        void UnregisterCVar(ICVar *cvar);

        void RegisterCommand(const std::string &name, const std::string &desc, const std::string &category, CommandFunc handler);
        void UnregisterCommand(const std::string &name);

        ICVar        *FindCVar(std::string_view name) const;
        CommandEntry *FindCommand(std::string_view name) const;

        std::vector<PrefixMatch> FindByPrefix(std::string_view prefix) const;

        void ForEachCVar(const std::function<void(ICVar *)> &fn) const;
        void ForEachCommand(const std::function<void(const CommandEntry &)> &fn) const;

    private:
        friend class Singleton<CommandRegistry>;
        CommandRegistry() = default;

        std::unordered_map<std::string, ICVar *>       cvars_;
        std::unordered_map<std::string, CommandEntry>   commands_;
    };

} // namespace sky
