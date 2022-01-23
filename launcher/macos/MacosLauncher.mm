//
// Created by Zach Lee on 2021/11/28.
//

#include "framework/Application.h"
#include <iostream>

void ProcessCommand(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        auto str = argv[i];
        std::cout << str << std::endl;
    }
}

int main(int argc, char** argv)
{
    ProcessCommand(argc, argv);

    sky::Application app;
    sky::StartInfo start = {};
    start.appName = "MacosLauncher";
    start.modules = {
        "SampleModule"
    };

    if (app.Init(start)) {
        app.Mainloop();
    }

    app.Shutdown();

    return 0;
}