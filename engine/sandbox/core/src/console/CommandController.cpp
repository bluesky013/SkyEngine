//
// Created on 2026/09/21.
//

#include <editor/core/console/CommandController.h>
#include <core/console/CVar.h>
#include <core/console/CommandRegistry.h>
#include <algorithm>

namespace sky::editor {

    bool CommandController::Execute(const std::string &line, std::string &outOutput)
    {
        if (line.empty()) {
            return false;
        }
        const CommandResult result = shell.Execute(line);
        history.Add(line);
        outOutput = result.output;
        return result.status == CommandResult::Status::OK;
    }

    std::string CommandController::HistoryPrevious()
    {
        return std::string(history.GetPrevious());
    }

    std::string CommandController::HistoryNext()
    {
        return std::string(history.GetNext());
    }

    void CommandController::ResetHistoryCursor()
    {
        history.ResetCursor();
    }

    std::vector<std::string> CommandController::Complete(const std::string &prefix) const
    {
        std::vector<std::string> out;
        if (prefix.empty()) {
            return out;
        }
        const auto matches = CommandRegistry::Get()->FindByPrefix(prefix);
        out.reserve(matches.size());
        for (const auto &match : matches) {
            if (match.cvar != nullptr) {
                out.emplace_back(match.cvar->GetName());
            } else if (match.cmd != nullptr) {
                out.push_back(match.cmd->name);
            }
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

} // namespace sky::editor
