//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/application/EditorApplication.h>
#include <core/environment/Environment.h>
#include <engine/SkyEngine.h>

namespace sky::editor {

    EditorApplication::EditorApplication()
    {

    }

    EditorApplication::~EditorApplication() noexcept
    {
        engine->DeInit();
        SkyEngine::Destroy();
    }

    bool EditorApplication::Init(StartInfo &info)
    {
        Application::Init(info);

        engine = SkyEngine::Get();
        engine->Init();

        BindTick([this](float delta) {
            engine->Tick(delta);
        });

        timer = new QTimer(this);
        timer->start(0);
        connect(timer, &QTimer::timeout, this, [this]() {
            Loop();
        });

        return true;
    }
}
