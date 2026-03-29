//
// Created by blues on 2025/5/25.
//

#pragma once

#include <core/console/ConsoleTypes.h>
#include <core/console/CommandToken.h>
#include <core/console/CommandRegistry.h>
#include <core/console/CVar.h>
#include <string>
#include <string_view>

namespace sky {

    class CommandShell {
    public:
        CommandShell();
        ~CommandShell() = default;

        CommandResult Execute(std::string_view input);
        CommandResult ExecFile(const std::string &path);

    private:
        void RegisterBuiltinCommands();

        CommandResult CmdHelp(CommandArgs args);
        CommandResult CmdFind(CommandArgs args);
        CommandResult CmdReset(CommandArgs args);
        CommandResult CmdEcho(CommandArgs args);
        CommandResult CmdExec(CommandArgs args);
    };

} // namespace sky
