//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/application/EditorApplication.h>
#include <engine/SkyEngine.h>
#include <framework/environment/Environment.h>

namespace sky::editor {

    void EditorApplication::Setup()
    {
        sky::StartInfo start = {};
        start.appName = "Editor";
        start.modules = {
        };

        Environment::Get();

        engine = SkyEngine::Get();
        engine->Init(start);
    }

    void EditorApplication::Shutdown()
    {
        if (engine != nullptr) {
            engine->DeInit();
            SkyEngine::Destroy();
            engine = nullptr;
        }
    }
}
