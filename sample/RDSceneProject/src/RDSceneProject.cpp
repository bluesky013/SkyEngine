//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <framework/asset/AssetManager.h>
#include <RDSceneProject/EngineRoot.h>
#include <filesystem>

namespace sky {

    static const char* BUFFER_PATH = "data/models/medi2_buffer.bin";
    static const char* MESH_PATH = "data/models/medi2_mesh0.mesh";

    void RDSceneProject::Init()
    {
        AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT);
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT);

        std::filesystem::path temp(BUFFER_PATH);
        auto str = temp.string();
        AssetManager::Get()->RegisterAsset(Uuid::CreateWithSeed(4059331220), BUFFER_PATH);
        AssetManager::Get()->RegisterAsset(Uuid::CreateWithSeed(Fnv1a32(MESH_PATH)), MESH_PATH);
    }

    void RDSceneProject::Start()
    {
        auto mesh = AssetManager::Get()->LoadAsset<Mesh>(Uuid::CreateWithSeed(Fnv1a32(MESH_PATH)));

    }

    void RDSceneProject::Stop()
    {

    }

    void RDSceneProject::Tick(float delta)
    {

    }

}

REGISTER_MODULE(sky::RDSceneProject)