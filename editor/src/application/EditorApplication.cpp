//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/application/EditorApplication.h>
#include <editor/document/Document.h>
#include <core/environment/Environment.h>
#include <engine/SkyEngine.h>

namespace sky::editor {

    static void EditorReflect()
    {
        Document::Reflect();
    }

    EditorApplication::EditorApplication()
    {
    }

    EditorApplication::~EditorApplication()
    {
        engine->DeInit();
        SkyEngine::Destroy();
    }

    bool EditorApplication::Init(StartInfo &info)
    {
        Application::Init(info);

        EditorReflect();
        SkyEngine::Reflect();

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
