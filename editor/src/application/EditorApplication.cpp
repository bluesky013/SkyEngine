//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/application/EditorApplication.h>
#include <engine/SkyEngine.h>
#include <core/environment/Environment.h>

namespace sky::editor {

    EditorApplication::~EditorApplication() noexcept
    {
        if (engine != nullptr) {
            engine->DeInit();
            SkyEngine::Destroy();
            engine = nullptr;
        }
    }

    void EditorApplication::Setup()
    {
        sky::StartInfo start = {};
        start.appName = "Editor";
        start.modules = {
        };

        Environment::Get();

        SkyEngine::Reflect();
        engine = SkyEngine::Get();
        engine->Init(start);
    }
}
