//
// Created by Zach Lee on 2023/1/15.
//

#include <editor/document/Level.h>
#include <engine/base/GameObject.h>
#include <engine/world/CameraComponent.h>
#include <framework/serialization/JsonArchive.h>
#include <fstream>
#include <render/Renderer.h>
#include <render/pipeline/DefaultForward.h>

#include <editor/viewport/Viewport.h>

namespace sky::editor {

    Level::~Level()
    {
        Save();
        Renderer::Get()->RemoveScene(renderScene);
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

        renderScene = Renderer::Get()->CreateScene();
        renderScene->SetPipeline(ppl);

    }

    const WorldPtr &Level::GetWorld() const
    {
        return world;
    }

} // namespace sky::editor
