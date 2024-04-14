//
// Created by Zach Lee on 2023/1/15.
//

#include <editor/document/Level.h>
#include <framework/world/Actor.h>
#include <framework/serialization/JsonArchive.h>
#include <fstream>
#include <render/Renderer.h>
#include <render/adaptor/pipeline/DefaultForward.h>
#include <render/adaptor/components/CameraComponent.h>

#include <editor/viewport/Viewport.h>

namespace sky::editor {

    Level::~Level()
    {
        Save();
    }

    void Level::New(const QString &level)
    {
        path = level;
        world = std::make_shared<World>();
        auto *go = world->CreateGameObject("MainCamera");
        go->AddComponent<CameraComponent>();
        InitRenderScene();
        Save();
    }

    void Level::Open(const QString &level)
    {
        path = level;
        world = std::make_shared<World>();
        InitRenderScene();
        Load();
    }

    void Level::Load()
    {
        auto str = path.toStdString();
        std::ifstream file(str, std::ios::binary);
        JsonInputArchive archive(file);
        archive.LoadValueObject(*world);
    }

    void Level::Save()
    {
        auto str = path.toStdString();
        std::ofstream file(str, std::ios::binary);
        JsonOutputArchive archive(file);
        archive.SaveValueObject(*world);
    }

    void Level::InitRenderScene()
    {
        auto *ppl = new DefaultForward();
        ppl->SetOutput(ViewportManager::Get()->FindViewport(ViewportID::EDITOR_PREVIEW)->GetRenderWindow());

        renderScene = std::make_unique<RenderSceneProxy>();
        renderScene->GetRenderScene()->SetPipeline(ppl);
        world->SetRenderScene(renderScene.get());
    }

    const WorldPtr &Level::GetWorld() const
    {
        return world;
    }

} // namespace sky::editor
