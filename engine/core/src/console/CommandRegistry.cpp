//
// Created by blues on 2025/5/25.
//

#include <core/console/CommandRegistry.h>
#include <core/console/CVar.h>
#include <algorithm>

namespace sky {

    void CommandRegistry::RegisterCVar(ICVar *cvar)
    {
        cvars_.emplace(std::string(cvar->GetName()), cvar);
    }

    void CommandRegistry::UnregisterCVar(ICVar *cvar)
    {
        cvars_.erase(std::string(cvar->GetName()));
    }

    void CommandRegistry::RegisterCommand(const std::string &name, const std::string &desc, const std::string &category, CommandFunc handler)
    {
        commands_.emplace(name, CommandEntry{name, desc, category, std::move(handler)});
    }

    void CommandRegistry::UnregisterCommand(const std::string &name)
    {
        commands_.erase(name);
    }

    ICVar *CommandRegistry::FindCVar(std::string_view name) const
    {
        auto it = cvars_.find(std::string(name));
        return it != cvars_.end() ? it->second : nullptr;
    }

    CommandEntry *CommandRegistry::FindCommand(std::string_view name) const
    {
        auto it = commands_.find(std::string(name));
        return it != commands_.end() ? const_cast<CommandEntry *>(&it->second) : nullptr;
    }

    std::vector<PrefixMatch> CommandRegistry::FindByPrefix(std::string_view prefix) const
    {
        std::vector<PrefixMatch> results;

        for (auto &[key, cvar] : cvars_) {
            if (HasFlag(cvar->GetFlags(), CVarFlags::HIDDEN)) {
                continue;
            }
            if (key.size() >= prefix.size() && std::string_view(key).substr(0, prefix.size()) == prefix) {
                results.push_back({cvar, nullptr});
            }
        }

        for (auto &[key, cmd] : commands_) {
            if (key.size() >= prefix.size() && std::string_view(key).substr(0, prefix.size()) == prefix) {
                results.push_back({nullptr, const_cast<CommandEntry *>(&cmd)});
            }
        }

        std::sort(results.begin(), results.end(), [](const PrefixMatch &a, const PrefixMatch &b) {
            std::string_view nameA = a.cvar ? a.cvar->GetName() : a.cmd->name;
            std::string_view nameB = b.cvar ? b.cvar->GetName() : b.cmd->name;
            return nameA < nameB;
        });

        return results;
    }

    void CommandRegistry::ForEachCVar(const std::function<void(ICVar *)> &fn) const
    {
        for (auto &[key, cvar] : cvars_) {
            fn(cvar);
        }
    }

    void CommandRegistry::ForEachCommand(const std::function<void(const CommandEntry &)> &fn) const
    {
        for (auto &[key, cmd] : commands_) {
            fn(cmd);
        }
    }

} // namespace sky
