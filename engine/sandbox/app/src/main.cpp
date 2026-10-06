//
// Created on 2026/09/21.
//

#include "EditorApplication.h"

#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>

#include <cstring>

#if defined(_WIN32)
#include <cstdio>
#include <windows.h>
#endif

using namespace sky;

namespace {
    // `--console` attaches a console window for live logs; by default the editor
    // is a GUI-subsystem app and opens none.
    bool WantsConsole(int argc, char **argv)
    {
        for (int i = 1; i < argc; ++i) {
            if (argv[i] != nullptr && std::strcmp(argv[i], "--console") == 0) {
                return true;
            }
        }
        return false;
    }

    void EnableConsole()
    {
#if defined(_WIN32)
        if (::GetConsoleWindow() == nullptr) {
            ::AllocConsole();
        }
        FILE *stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        freopen_s(&stream, "CONIN$", "r", stdin);
#endif
    }
} // namespace

int main(int argc, char **argv)
{
    if (WantsConsole(argc, argv)) {
        EnableConsole();
    }

    if (!Platform::Get()->Init({})) {
        return -1;
    }

    editor::sandbox::EditorApplication app;
    if (app.Init(argc, argv)) {
        app.Mainloop();
    }
    return 0;
}
