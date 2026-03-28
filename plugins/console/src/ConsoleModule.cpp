//
// Created by blues on 2025/5/25.
//

#include <console/ConsoleModule.h>
#include <core/console/CommandRegistry.h>
#include <core/console/CVar.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <framework/application/SettingRegistry.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <core/cmdline/CmdParser.h>
#include <filesystem>

namespace sky {

    bool ConsoleModule::Init(const StartArguments &args)
    {
        LOG_I("Console", "ConsoleModule::Init -- IsTTY will be checked after IO creation");

        // parse --exec arguments
        CmdOptions options("ConsoleModule", "Console System");
        options.allow_unrecognised_options();
        options.add_options()("exec", "Execute cfg file", CmdValue<std::vector<std::string>>());

        if (!args.args.empty()) {
            auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
            if (result.count("exec") != 0u) {
                execPaths_ = result["exec"].as<std::vector<std::string>>();
            }
        }

        // determine history path
        historyPath_ = ".skyengine/console_history";

        // create console log and install logger hook
        consoleLog_ = std::make_unique<ConsoleLog>();
        consoleLog_->Install();

        // create command shell
        shell_ = std::make_unique<CommandShell>();

        // create command history
        history_ = std::make_unique<CommandHistory>();
        history_->Load(historyPath_);

        // create terminal IO (platform-specific)
        terminalIO_.reset(ITerminalIO::Create());

        // create terminal
        terminal_ = std::make_unique<ConsoleTerminal>(terminalIO_.get());
        terminal_->SetHistory(history_.get());

        if (terminal_->IsTTY()) {
            terminalIO_->EnableRawMode();
            LOG_I("Console", "TTY detected, raw mode enabled");
        } else {
            LOG_I("Console", "No TTY detected (pipe/redirected), line mode");
        }

        // register system commands
        RegisterBuiltinSystemCommands();

        // sync ARCHIVE CVars from SettingRegistry
        SyncArchiveCVarsFromSettings();

        terminal_->DrawPrompt();

        return true;
    }

    void ConsoleModule::Start()
    {
        ProcessExecArgs();
    }

    void ConsoleModule::Tick(float delta)
    {
        // flush log entries to terminal
        if (consoleLog_ && terminal_) {
            consoleLog_->FlushPendingEntries([this](const LogEntry &entry) {
                terminal_->WriteLogEntry(entry);
            });
        }

        // poll terminal input
        std::string line;
        if (terminal_ && terminal_->PollInput(line)) {
            if (!line.empty()) {
                history_->Add(line);
                history_->ResetCursor();
                auto result = shell_->Execute(line);
                if (!result.output.empty()) {
                    if (terminal_->IsTTY()) {
                        terminalIO_->Write(result.output + "\n");
                    } else {
                        terminalIO_->Write(result.output + "\n");
                    }
                }
            }
            terminal_->DrawPrompt();
        }
    }

    void ConsoleModule::Shutdown()
    {
        // save history
        if (history_) {
            history_->Save(historyPath_);
        }

        // sync ARCHIVE CVars to SettingRegistry
        SyncArchiveCVarsToSettings();

        // restore terminal mode
        if (terminalIO_) {
            terminalIO_->RestoreMode();
        }

        // uninstall logger hook
        if (consoleLog_) {
            consoleLog_->Uninstall();
        }

        // unregister system commands
        auto *registry = CommandRegistry::Get();
        registry->UnregisterCommand("sys.exit");
        registry->UnregisterCommand("sys.version");
    }

    void ConsoleModule::RegisterBuiltinSystemCommands()
    {
        auto *registry = CommandRegistry::Get();

        registry->RegisterCommand("sys.exit", "Exit engine", "sys", [](CommandArgs) -> CommandResult {
            auto *notify = Interface<ISystemNotify>::Get()->GetApi();
            if (notify != nullptr) {
                notify->SetExit();
            }
            return {CommandResult::Status::OK, "Shutting down..."};
        });

        registry->RegisterCommand("sys.version", "Show engine version", "sys", [](CommandArgs) -> CommandResult {
            return {CommandResult::Status::OK, "SkyEngine"};
        });
    }

    void ConsoleModule::SyncArchiveCVarsFromSettings()
    {
        // Try to read from SettingRegistry
        auto *registry = CommandRegistry::Get();
        registry->ForEachCVar([](ICVar *cvar) {
            if (!HasFlag(cvar->GetFlags(), CVarFlags::ARCHIVE)) {
                return;
            }
            // SettingRegistry is not a singleton -- it's owned by Application
            // For now, we skip this. TODO: integrate with SettingRegistry when accessible
        });
    }

    void ConsoleModule::SyncArchiveCVarsToSettings()
    {
        auto *registry = CommandRegistry::Get();
        registry->ForEachCVar([](ICVar *cvar) {
            if (!HasFlag(cvar->GetFlags(), CVarFlags::ARCHIVE)) {
                return;
            }
            // TODO: write to SettingRegistry
        });
    }

    void ConsoleModule::ProcessExecArgs()
    {
        for (const auto &path : execPaths_) {
            auto result = shell_->ExecFile(path);
            if (result.status != CommandResult::Status::OK) {
                LOG_W("Console", "%s", result.output.c_str());
            } else {
                LOG_I("Console", "%s", result.output.c_str());
            }
        }
    }

} // namespace sky

