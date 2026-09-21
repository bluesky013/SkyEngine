//
// Created on 2026/09/21.
//

#pragma once

#include <framework/console/CommandHistory.h>
#include <framework/console/CommandShell.h>
#include <string>
#include <vector>

namespace sky::editor {

    // Toolkit-independent command-line controller for the editor console.
    //
    // Executes submitted lines through CommandShell, records history, and
    // offers completion candidates from CommandRegistry. A panel provides the
    // text input and renders the output.
    class CommandController {
    public:
        CommandController() = default;
        ~CommandController() = default;

        CommandController(const CommandController &) = delete;
        CommandController &operator=(const CommandController &) = delete;

        // Executes a submitted line. Empty input is a no-op and returns false.
        bool Execute(const std::string &line, std::string &outOutput);

        std::string HistoryPrevious();
        std::string HistoryNext();
        void ResetHistoryCursor();

        // Completion candidates (CVars + commands) for the input prefix.
        std::vector<std::string> Complete(const std::string &prefix) const;

        CommandHistory &GetHistory() { return history; }
        const CommandHistory &GetHistory() const { return history; }

    private:
        CommandShell shell;
        CommandHistory history;
    };

} // namespace sky::editor
