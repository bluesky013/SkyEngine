//
// Created by blues on 2025/5/25.
//

#include <framework/console/CommandShell.h>
#include <core/logger/Logger.h>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace sky {

    CommandShell::CommandShell()
    {
        RegisterBuiltinCommands();
    }

    CommandResult CommandShell::Execute(std::string_view input)
    {
        auto tokens = TokenizeCommand(input);
        if (tokens.empty()) {
            return {CommandResult::Status::OK, ""};
        }

        const auto &name = tokens[0];
        auto *registry = CommandRegistry::Get();

        // try CVar first
        auto *cvar = registry->FindCVar(name);
        if (cvar != nullptr) {
            if (tokens.size() == 1) {
                // CVar get
                std::string output = std::string(cvar->GetName()) + " = " + cvar->ToString()
                    + "  (" + std::string(cvar->GetTypeName()) + ", \"" + std::string(cvar->GetDesc()) + "\")";
                return {CommandResult::Status::OK, std::move(output)};
            }
            // CVar set
            std::string oldVal = cvar->ToString();
            if (!cvar->SetFromString(tokens[1])) {
                if (HasFlag(cvar->GetFlags(), CVarFlags::READ_ONLY)) {
                    return {CommandResult::Status::ERROR, std::string(cvar->GetName()) + " is read-only"};
                }
                return {CommandResult::Status::ERROR, "Invalid value \"" + tokens[1] + "\" for " + std::string(cvar->GetName())};
            }
            std::string output = std::string(cvar->GetName()) + " = " + cvar->ToString() + " (was: " + oldVal + ")";
            return {CommandResult::Status::OK, std::move(output)};
        }

        // try command
        auto *cmd = registry->FindCommand(name);
        if (cmd != nullptr) {
            std::vector<std::string_view> argViews;
            for (size_t i = 1; i < tokens.size(); ++i) {
                argViews.emplace_back(tokens[i]);
            }
            return cmd->handler(CommandArgs{argViews});
        }

        return {CommandResult::Status::NOT_FOUND, "Unknown command: " + name};
    }

    CommandResult CommandShell::ExecFile(const std::string &path)
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            return {CommandResult::Status::ERROR, "File not found: " + path};
        }

        int executed = 0;
        std::string line;
        while (std::getline(file, line)) {
            // skip empty or whitespace-only lines
            auto first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                continue;
            }
            // skip comments
            if (line.size() >= first + 2 && line[first] == '/' && line[first + 1] == '/') {
                continue;
            }
            Execute(line);
            ++executed;
        }

        std::string summary = "Executed " + std::to_string(executed) + " commands from " + path;
        return {CommandResult::Status::OK, std::move(summary)};
    }

    void CommandShell::RegisterBuiltinCommands()
    {
        auto *registry = CommandRegistry::Get();

        registry->RegisterCommand("help", "List commands and CVars", "sys",
            [this](CommandArgs args) { return CmdHelp(args); });
        registry->RegisterCommand("find", "Search CVars/commands by substring", "sys",
            [this](CommandArgs args) { return CmdFind(args); });
        registry->RegisterCommand("reset", "Reset a CVar to its default value", "sys",
            [this](CommandArgs args) { return CmdReset(args); });
        registry->RegisterCommand("echo", "Print arguments to console", "sys",
            [this](CommandArgs args) { return CmdEcho(args); });
        registry->RegisterCommand("exec", "Execute a .cfg file", "sys",
            [this](CommandArgs args) { return CmdExec(args); });
    }

    CommandResult CommandShell::CmdHelp(CommandArgs args)
    {
        auto *registry = CommandRegistry::Get();
        std::ostringstream out;

        std::string filter;
        if (!args.empty()) {
            filter = std::string(args[0]);
        }

        // check if filter matches a specific command/cvar
        if (!filter.empty()) {
            auto *cvar = registry->FindCVar(filter);
            if (cvar != nullptr) {
                out << std::string(cvar->GetName()) << " (" << std::string(cvar->GetTypeName()) << ") = " << cvar->ToString()
                    << " -- " << std::string(cvar->GetDesc());
                return {CommandResult::Status::OK, out.str()};
            }
            auto *cmd = registry->FindCommand(filter);
            if (cmd != nullptr) {
                out << cmd->name << " -- " << cmd->desc;
                return {CommandResult::Status::OK, out.str()};
            }
        }

        // list by category or all
        bool first = true;
        registry->ForEachCVar([&](ICVar *cvar) {
            if (!filter.empty() && cvar->GetCategory() != filter) {
                return;
            }
            if (HasFlag(cvar->GetFlags(), CVarFlags::HIDDEN)) {
                return;
            }
            if (!first) out << "\n";
            first = false;
            out << "  " << std::string(cvar->GetName()) << " (" << std::string(cvar->GetTypeName()) << ") = " << cvar->ToString()
                << " -- " << std::string(cvar->GetDesc());
        });

        registry->ForEachCommand([&](const CommandEntry &cmd) {
            if (!filter.empty() && cmd.category != filter) {
                return;
            }
            if (!first) out << "\n";
            first = false;
            out << "  " << cmd.name << " -- " << cmd.desc;
        });

        return {CommandResult::Status::OK, out.str()};
    }

    CommandResult CommandShell::CmdFind(CommandArgs args)
    {
        if (args.empty()) {
            return {CommandResult::Status::ERROR, "Usage: find <substring>"};
        }

        std::string needle(args[0]);
        std::string lowerNeedle = needle;
        std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        auto *registry = CommandRegistry::Get();
        std::ostringstream out;
        bool first = true;

        auto containsIgnoreCase = [&](std::string_view haystack) {
            std::string lower(haystack);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower.find(lowerNeedle) != std::string::npos;
        };

        registry->ForEachCVar([&](ICVar *cvar) {
            if (containsIgnoreCase(cvar->GetName()) || containsIgnoreCase(cvar->GetDesc())) {
                if (!first) out << "\n";
                first = false;
                out << "  " << std::string(cvar->GetName()) << " (" << std::string(cvar->GetTypeName()) << ") = " << cvar->ToString()
                    << " -- " << std::string(cvar->GetDesc());
            }
        });

        registry->ForEachCommand([&](const CommandEntry &cmd) {
            if (containsIgnoreCase(cmd.name) || containsIgnoreCase(cmd.desc)) {
                if (!first) out << "\n";
                first = false;
                out << "  " << cmd.name << " -- " << cmd.desc;
            }
        });

        if (first) {
            return {CommandResult::Status::OK, "No matches found for \"" + needle + "\""};
        }
        return {CommandResult::Status::OK, out.str()};
    }

    CommandResult CommandShell::CmdReset(CommandArgs args)
    {
        if (args.empty()) {
            return {CommandResult::Status::ERROR, "Usage: reset <cvar_name>"};
        }

        auto *cvar = CommandRegistry::Get()->FindCVar(args[0]);
        if (cvar == nullptr) {
            return {CommandResult::Status::NOT_FOUND, "Unknown CVar: " + std::string(args[0])};
        }

        cvar->ResetToDefault();
        return {CommandResult::Status::OK, std::string(cvar->GetName()) + " reset to " + cvar->ToString()};
    }

    CommandResult CommandShell::CmdEcho(CommandArgs args)
    {
        std::string result;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) result += ' ';
            result += args[i];
        }
        return {CommandResult::Status::OK, result};
    }

    CommandResult CommandShell::CmdExec(CommandArgs args)
    {
        if (args.empty()) {
            return {CommandResult::Status::ERROR, "Usage: exec <path>"};
        }
        return ExecFile(std::string(args[0]));
    }

} // namespace sky

