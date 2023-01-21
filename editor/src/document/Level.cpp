//
// Created by Zach Lee on 2023/1/15.
//

#include <editor/document/Level.h>
#include <fstream>
#include <engine/base/GameObject.h>
#include <engine/world/CameraComponent.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>

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
        Save();
    }

    void Level::Open(const QString &level)
    {
        path = level;
        world = std::make_shared<World>();
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

    const WorldPtr &Level::GetWorld() const
    {
        return world;
    }

} // namespace sky::editor
