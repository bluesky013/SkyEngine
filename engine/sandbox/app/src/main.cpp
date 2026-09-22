//
// Created on 2026/09/21.
//

#include "EditorApplication.h"

#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>

using namespace sky;

int main(int argc, char **argv)
{
    if (!Platform::Get()->Init({})) {
        return -1;
    }

    editor::sandbox::EditorApplication app;
    if (app.Init(argc, argv)) {
        app.Mainloop();
    }
    return 0;
}
