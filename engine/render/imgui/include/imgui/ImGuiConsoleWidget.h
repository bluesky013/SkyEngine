//
// Created by Copilot on 2026/3/29.
//

#pragma once

#include <imgui/ImWidget.h>
#include <framework/console/IConsoleUI.h>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace sky {

    class ImGuiConsoleWidget : public ImWidget, public IConsoleUI {
    public:
        ImGuiConsoleWidget();
        ~ImGuiConsoleWidget() override;

        // ImWidget
        void Execute(ImContext &context) override;

        // IConsoleUI - visibility
        void Toggle() override;
        void Show() override;
        void Hide() override;
        bool IsVisible() const override;
        bool WantsInput() const override;

        // IConsoleUI - data
        void PushLog(const LogEntry &entry) override;
        bool PollCommand(std::string &outLine) override;
        void PushOutput(const std::string &text) override;

    private:
        struct DisplayEntry {
            std::string text;
            std::string level;
        };

        void SubmitCommand();

        static int InputTextCallback(ImGuiInputTextCallbackData *data);
        int HandleInputCallback(ImGuiInputTextCallbackData *data);

        bool visible        = false;
        bool scrollToBottom = false;
        bool focusInput     = false;
        bool prevGraveDown  = false;
        bool suppressToggleCharacter = false;

        mutable std::mutex mutex;
        std::vector<DisplayEntry> entries;
        std::queue<std::string>   pendingCommands;

        char inputBuf[512] = {};

        // command history (widget-local)
        std::vector<std::string> history;
        int historyPos = -1;

        // tab completion
        std::vector<std::string> completionCandidates;
        bool showCompletions = false;

        static constexpr size_t MAX_ENTRIES = 2048;
    };

} // namespace sky
