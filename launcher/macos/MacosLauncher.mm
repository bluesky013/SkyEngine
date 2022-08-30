//
// Created by Zach Lee on 2021/11/28.
//

#include "Command.h"
#include <framework/application/Application.h>
#include <iostream>

int main(int argc, char **argv) {
  sky::CommandInfo cmdInfo = {};
  sky::ProcessCommand(argc, argv, cmdInfo);

  sky::StartInfo start = {};
  start.appName = "MacosLauncher";
  start.modules.swap(cmdInfo.modules);

  sky::Application app;
  if (app.Init(start)) {
    app.Mainloop();
  }

  app.Shutdown();

  return 0;
}