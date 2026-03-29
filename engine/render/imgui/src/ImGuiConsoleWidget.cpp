//
// Created by Copilot on 2026/3/29.
//

#include <imgui/ImGuiConsoleWidget.h>
#include <framework/console/ConsoleLog.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <core/console/CommandRegistry.h>
#include <core/console/CVar.h>
#include <imgui.h>
#include <algorithm>

namespace sky {

    ImGuiConsoleWidget::ImGuiConsoleWidget()
        : ImWidget("ConsoleOverlay")
    {
        Interface<IConsoleUI>::Get()->Register(*this);
    }

    ImGuiConsoleWidget::~ImGuiConsoleWidget()
    {
        Interface<IConsoleUI>::Get()->UnRegister(*this);
    }

    // -- visibility ----------------------------------------------------------

    void ImGuiConsoleWidget::Toggle()
    {
        visible = !visible;
        if (visible) {
            focusInput = true;
        }
    }

    void ImGuiConsoleWidget::Show()
    {
        visible = true;
        focusInput = true;
    }

    void ImGuiConsoleWidget::Hide()
    {
        visible = false;
    }

    bool ImGuiConsoleWidget::IsVisible() const { return visible; }
    bool ImGuiConsoleWidget::WantsInput() const { return visible; }

    // -- data ----------------------------------------------------------------

    void ImGuiConsoleWidget::PushLog(const LogEntry &entry)
    {
        DisplayEntry de;
        if (!entry.tag.empty()) {
            de.text = "[" + entry.tag + "] " + entry.message;
        } else {
            de.text = entry.message;
        }
        de.level = entry.level;

        std::lock_guard<std::mutex> lock(mutex);
        entries.push_back(std::move(de));
        if (entries.size() > MAX_ENTRIES) {
            entries.erase(entries.begin());
        }
    }

    bool ImGuiConsoleWidget::PollCommand(std::string &outLine)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (pendingCommands.empty()) {
            return false;
        }
        outLine = std::move(pendingCommands.front());
        pendingCommands.pop();
        return true;
    }

    void ImGuiConsoleWidget::PushOutput(const std::string &text)
    {
        DisplayEntry de;
        de.text  = "> " + text;
        de.level = "OUTPUT";

        std::lock_guard<std::mutex> lock(mutex);
        entries.push_back(std::move(de));
        if (entries.size() > MAX_ENTRIES) {
            entries.erase(entries.begin());
        }
    }

    // -- rendering -----------------------------------------------------------

    void ImGuiConsoleWidget::Execute(ImContext &context)
    {
        // Tilde / grave accent toggle (edge-detect)
        ImGuiIO &io = ImGui::GetIO();
        const auto graveIdx = static_cast<int>(ScanCode::KEY_GRAVE);
        const bool graveDown = graveIdx < IM_ARRAYSIZE(io.KeysDown) && io.KeysDown[graveIdx];

        if (graveDown && !prevGraveDown) {
            Toggle();
            suppressToggleCharacter = true;
            // consume the key so it does not propagate
            io.KeysDown[graveIdx] = false;
        }
        prevGraveDown = graveDown;

        if (!visible) {
            return;
        }

        // layout
        const ImVec2 displaySize = io.DisplaySize;
        const float windowHeight = displaySize.y * 0.5f;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(displaySize.x, windowHeight));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar    |
            ImGuiWindowFlags_NoResize      |
            ImGuiWindowFlags_NoMove        |
            ImGuiWindowFlags_NoCollapse    |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.85f));

        if (ImGui::Begin("##ConsoleOverlay", nullptr, flags)) {
            // reserve space for input and optional completion list
            const float inputHeight = ImGui::GetFrameHeightWithSpacing();
            const float completionHeight = (showCompletions && !completionCandidates.empty())
                ? ImGui::GetTextLineHeightWithSpacing() *
                  static_cast<float>(std::min(completionCandidates.size(), static_cast<size_t>(5))) +
                  ImGui::GetFrameHeightWithSpacing()
                : 0.0f;

            // -- log area ----------------------------------------------------
            ImGui::PushAllowKeyboardFocus(false);
            ImGui::BeginChild("##LogArea", ImVec2(0, -inputHeight - completionHeight), false);
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (const auto &entry : entries) {
                    ImVec4 color(1.0f, 1.0f, 1.0f, 1.0f);
                    if (entry.level == "ERROR") {
                        color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    } else if (entry.level == "WARNING") {
                        color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
                    } else if (entry.level == "OUTPUT") {
                        color = ImVec4(0.6f, 0.9f, 1.0f, 1.0f);
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextWrapped("%s", entry.text.c_str());
                    ImGui::PopStyleColor();
                }
            }

            // auto-scroll: stay at bottom unless the user scrolled up
            if (scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            scrollToBottom = false;

            ImGui::EndChild();

            // -- completion candidates ---------------------------------------
            if (showCompletions && !completionCandidates.empty()) {
                ImGui::BeginChild("##Completions", ImVec2(0, completionHeight), true);
                for (const auto &candidate : completionCandidates) {
                    ImGui::TextUnformatted(candidate.c_str());
                }
                ImGui::EndChild();
            }
            ImGui::PopAllowKeyboardFocus();

            // -- input field -------------------------------------------------
            const ImGuiInputTextFlags inputFlags =
                ImGuiInputTextFlags_EnterReturnsTrue   |
                ImGuiInputTextFlags_CallbackHistory    |
                ImGuiInputTextFlags_CallbackCompletion |
                ImGuiInputTextFlags_CallbackCharFilter;

            if (focusInput) {
                ImGui::SetKeyboardFocusHere();
                focusInput = false;
            }

            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##ConsoleInput", inputBuf, sizeof(inputBuf), inputFlags,
                                 InputTextCallback, this)) {
                SubmitCommand();
            }
            ImGui::PopItemWidth();
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    // -- input helpers -------------------------------------------------------

    void ImGuiConsoleWidget::SubmitCommand()
    {
        std::string line(inputBuf);
        inputBuf[0] = '\0';

        if (!line.empty()) {
            history.push_back(line);
            historyPos = -1;

            std::lock_guard<std::mutex> lock(mutex);
            pendingCommands.push(std::move(line));
        }
        showCompletions = false;
        scrollToBottom = true;
        focusInput = true;
    }

    int ImGuiConsoleWidget::InputTextCallback(ImGuiInputTextCallbackData *data)
    {
        auto *self = static_cast<ImGuiConsoleWidget *>(data->UserData);
        return self->HandleInputCallback(data);
    }

    int ImGuiConsoleWidget::HandleInputCallback(ImGuiInputTextCallbackData *data)
    {
        // filter grave/tilde characters that leak from the toggle key
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
            const bool suppressToggleChar = suppressToggleCharacter;
            suppressToggleCharacter = false;
            if (suppressToggleChar && (data->EventChar == '`' || data->EventChar == '~')) {
                return 1; // reject
            }
            return 0;
        }

        // history navigation (Up / Down)
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            const int prevPos = historyPos;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (historyPos == -1) {
                    historyPos = static_cast<int>(history.size()) - 1;
                } else if (historyPos > 0) {
                    historyPos--;
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (historyPos >= 0) {
                    historyPos++;
                }
                if (historyPos >= static_cast<int>(history.size())) {
                    historyPos = -1;
                }
            }

            if (historyPos != prevPos) {
                data->DeleteChars(0, data->BufTextLen);
                if (historyPos >= 0 && historyPos < static_cast<int>(history.size())) {
                    data->InsertChars(0, history[historyPos].c_str());
                }
            }

            showCompletions = false;
            return 0;
        }

        // tab completion
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            const std::string_view prefix(data->Buf, static_cast<size_t>(data->CursorPos));
            if (prefix.empty()) {
                return 0;
            }

            const auto matches = CommandRegistry::Get()->FindByPrefix(prefix);
            completionCandidates.clear();
            for (const auto &m : matches) {
                if (m.cmd != nullptr) {
                    completionCandidates.push_back(m.cmd->name);
                }
                if (m.cvar != nullptr) {
                    completionCandidates.push_back(std::string(m.cvar->GetName()));
                }
            }

            if (completionCandidates.size() == 1) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, completionCandidates[0].c_str());
                showCompletions = false;
            } else if (!completionCandidates.empty()) {
                showCompletions = true;
            } else {
                showCompletions = false;
            }
            return 0;
        }

        return 0;
    }

} // namespace sky
