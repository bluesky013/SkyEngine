//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/application/EditorApplication.h>

namespace sky::editor {

    void EditorApplication::Setup()
    {
        sky::StartInfo start = {};
        start.appName = "Editor";
        start.modules = {
        };

        engineInstance = new SkyEngine();
        engineInstance->Init(start);
    }

    void EditorApplication::Shutdown()
    {
        if (engineInstance != nullptr) {
            engineInstance->DeInit();
        }
        delete engineInstance;
        engineInstance = nullptr;
    }
}
