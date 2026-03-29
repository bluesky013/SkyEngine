//
// Created by blues on 2025/5/25.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/console/CommandShell.h>
#include <framework/console/CommandHistory.h>
#include <framework/console/ConsoleLog.h>
#include <framework/console/ConsoleTerminal.h>
#include <framework/console/ITerminalIO.h>
#include <memory>

namespace sky {

    class ConsoleModule : public IModule {
    public:
        ConsoleModule() = default;
        ~ConsoleModule() override = default;

        bool Init(const StartArguments &args) override;
        void Start() override;
        void Tick(float delta) override;
        void Shutdown() override;

    private:
        void RegisterBuiltinSystemCommands();
        void SyncArchiveCVarsFromSettings();
        void SyncArchiveCVarsToSettings();
        void ProcessExecArgs();

        std::unique_ptr<ConsoleLog>      consoleLog_;
        std::unique_ptr<CommandShell>    shell_;
        std::unique_ptr<CommandHistory>  history_;
        std::unique_ptr<ITerminalIO>     terminalIO_;
        std::unique_ptr<ConsoleTerminal> terminal_;

        std::vector<std::string> execPaths_;
        std::string historyPath_;
    };

} // namespace sky
